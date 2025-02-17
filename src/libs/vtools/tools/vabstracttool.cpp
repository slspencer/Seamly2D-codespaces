/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017  Seamly, LLC                                       *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 **************************************************************************

 ************************************************************************
 **
 **  @file   vabstracttool.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   November 15, 2013
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentine project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013-2015 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "vabstracttool.h"

#include <QBrush>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFlags>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QHash>
#include <QIcon>
#include <QLineF>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSharedPointer>
#include <QStaticStringData>
#include <QStringData>
#include <QStringDataPtr>
#include <QStyle>
#include <QUndoStack>
#include <QVector>
#include <new>
#include <qnumeric.h>

#include "../vgeometry/vpointf.h"
#include "../vpropertyexplorer/checkablemessagebox.h"
#include "../vwidgets/vmaingraphicsview.h"
#include "../ifc/exception/vexception.h"
#include "../ifc/exception/vexceptionundo.h"
#include "../ifc/xml/vtoolrecord.h"
#include "../undocommands/deltool.h"
#include "../vgeometry/../ifc/ifcdef.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vcubicbezier.h"
#include "../vgeometry/vcubicbezierpath.h"
#include "../vgeometry/vsplinepath.h"
#include "../vgeometry/varc.h"
#include "../vgeometry/vellipticalarc.h"
#include "../vmisc/vcommonsettings.h"
#include "../vmisc/logging.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/calculator.h"
#include "../vwidgets/vgraphicssimpletextitem.h"
#include "nodeDetails/nodedetails.h"
#include "../dialogs/support/dialogundo.h"
#include "../dialogs/support/edit_formula_dialog.h"

template <class T> class QSharedPointer;

bool VAbstractTool::m_suppressContextMenu = false;
const QString VAbstractTool::AttrInUse = QStringLiteral("inUse");

namespace
{
//---------------------------------------------------------------------------------------------------------------------
quint32 CreateNodeSpline(VContainer *data, quint32 id)
{
    if (data->GetGObject(id)->getType() == GOType::Spline)
    {
        return VAbstractTool::CreateNode<VSpline>(data, id);
    }
    else
    {
        return VAbstractTool::CreateNode<VCubicBezier>(data, id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
quint32 CreateNodeSplinePath(VContainer *data, quint32 id)
{
    if (data->GetGObject(id)->getType() == GOType::SplinePath)
    {
        return VAbstractTool::CreateNode<VSplinePath>(data, id);
    }
    else
    {
        return VAbstractTool::CreateNode<VCubicBezierPath>(data, id);
    }
}
}//static functions

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief VAbstractTool container.
 * @param doc dom document container.
 * @param data container with data.
 * @param id object id in container.
 * @param parent parent object.
 */
VAbstractTool::VAbstractTool(VAbstractPattern *doc, VContainer *data, quint32 id, QObject *parent)
    : VDataTool(data, parent)
    , doc(doc)
    , m_id(id)
    , vis()
    , selectionType(SelectionType::ByMouseRelease)
{
    SCASSERT(doc != nullptr)
    connect(this, &VAbstractTool::toolHasChanges, this->doc, &VAbstractPattern::haveLiteChange);
    connect(this->doc, &VAbstractPattern::FullUpdateFromFile, this, &VAbstractTool::FullUpdateFromFile);
    connect(this, &VAbstractTool::LiteUpdateTree, this->doc, &VAbstractPattern::LiteParseTree);
}

//---------------------------------------------------------------------------------------------------------------------
VAbstractTool::~VAbstractTool()
{
    if (not vis.isNull())
    {
        delete vis;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief CheckFormula check formula.
 *
 * Try calculate formula. If find error show dialog that allow user try fix formula. If user can't throw exception. In
 * successes case return result calculation and fixed formula string. If formula ok don't touch formula.
 *
 * @param toolId [in] tool's id.
 * @param formula [in|out] string with formula.
 * @param data [in] container with variables. Need for math parser.
 * @throw QmuParserError.
 * @return result of calculation formula.
 */
qreal VAbstractTool::CheckFormula(const quint32 &toolId, QString &formula, VContainer *data)
{
    SCASSERT(data != nullptr)
    qreal result = 0;
    try
    {
        QScopedPointer<Calculator> cal(new Calculator());
        result = cal->EvalFormula(data->DataVariables(), formula);

        if (qIsInf(result) || qIsNaN(result))
        {
            qDebug() << "Invalid the formula value";
            return 0;
        }
    }
    catch (qmu::QmuParserError &e)
    {
        qDebug() << "\nMath parser error:\n"
                 << "--------------------------------------\n"
                 << "Message:     " << e.GetMsg()  << "\n"
                 << "Expression:  " << e.GetExpr() << "\n"
                 << "--------------------------------------";

        if (qApp->IsAppInGUIMode())
        {
            QScopedPointer<DialogUndo> dialogUndo(new DialogUndo(qApp->getMainWindow()));
            forever
            {
                if (dialogUndo->exec() == QDialog::Accepted)
                {
                    const UndoButton resultUndo = dialogUndo->Result();
                    if (resultUndo == UndoButton::Fix)
                    {
                        auto *dialog = new EditFormulaDialog(data, toolId, qApp->getMainWindow());
                        dialog->setWindowTitle(tr("Edit wrong formula"));
                        dialog->SetFormula(formula);
                        if (dialog->exec() == QDialog::Accepted)
                        {
                            formula = dialog->GetFormula();
                            /* Need delete dialog here because parser in dialog don't allow use correct separator for
                             * parsing here. */
                            delete dialog;
                            QScopedPointer<Calculator> cal1(new Calculator());
                            result = cal1->EvalFormula(data->DataVariables(), formula);

                            if (qIsInf(result) || qIsNaN(result))
                            {
                                qDebug() << "Invalid the formula value";
                                return 0;
                            }

                            break;
                        }
                        else
                        {
                            delete dialog;
                        }
                    }
                    else
                    {
                        throw VExceptionUndo(QString("Undo wrong formula %1").arg(formula));
                    }
                }
                else
                {
                    throw;
                }
            }
        }
        else
        {
            throw;
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief deleteTool full delete object form scene and file.
 */
void VAbstractTool::deleteTool(bool ask)
{
    qCDebug(vTool, "Deleting abstract tool.");
    if (_referens <= 1)
    {
        qCDebug(vTool, "No children.");
        qApp->getSceneView()->itemClicked(nullptr);
        if (ask)
        {
            qCDebug(vTool, "Asking.");
            if (ConfirmDeletion() == QMessageBox::No)
            {
                qCDebug(vTool, "User said no.");
                return;
            }
        }

        qCDebug(vTool, "Begin deleting.");
        DelTool *delTool = new DelTool(doc, m_id);
        connect(delTool, &DelTool::NeedFullParsing, doc, &VAbstractPattern::NeedFullParsing);
        qApp->getUndoStack()->push(delTool);

        // Throw exception, this will help prevent case when we forget to immediately quit function.
        VExceptionToolWasDeleted e("Tool was used after deleting.");
        throw e;
    }
    else
    {
        qCDebug(vTool, "Can't delete, tool has children.");
    }
}

//---------------------------------------------------------------------------------------------------------------------
int VAbstractTool::ConfirmDeletion()
{
    if (false == qApp->Settings()->getConfirmItemDelete())
    {
        return QMessageBox::Yes;
    }

    Utils::CheckableMessageBox msgBox(qApp->getMainWindow());
    msgBox.setWindowTitle(tr("Confirm deletion"));
    msgBox.setText(tr("Do you really want to delete?"));
    msgBox.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    msgBox.setDefaultButton(QDialogButtonBox::No);
    msgBox.setIconPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(32, 32) );

    int dialogResult = msgBox.exec();

    if (dialogResult == QDialog::Accepted)
    {
        qApp->Settings()->setConfirmItemDelete(not msgBox.isChecked());
    }

    return dialogResult == QDialog::Accepted ? QMessageBox::Yes : QMessageBox::No;
}

//---------------------------------------------------------------------------------------------------------------------
const QStringList VAbstractTool::Colors()
{
    const QStringList colors = QStringList() << ColorBlack          << ColorGreen           << ColorBlue
                                             << ColorDarkRed        << ColorDarkGreen       << ColorDarkBlue
                                             << ColorYellow         << ColorLightSalmon     << ColorGoldenRod
                                             << ColorOrange         << ColorDeepPink        << ColorViolet
                                             << ColorDarkViolet     << ColorMediumSeaGreen  << ColorLime
                                             << ColorDeepSkyBlue    << ColorCornFlowerBlue;
    return colors;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::ColorsList()
{
    QMap<QString, QString> map;

    const QStringList colorNames = Colors();
    for (int i = 0; i < colorNames.size(); ++i)
    {
        QString name;
        switch (i)
        {
            case 1: // ColorGreen
                name = tr("Green");
                break;
            case 2: // ColorBlue
                name = tr("Blue");
                break;
            case 3: // ColorDarkRed
                name = tr("Dark Red");
                break;
            case 4: // ColorDarkGreen
                name = tr("Dark Green");
                break;
            case 5: // ColorDarkBlue
                name = tr("Dark Blue");
                break;
            case 6: // ColorYellow
                name = tr("Yellow");
                break;
            case 7: // ColorLightSalmon
                name = tr("Light Salmon");
                break;
            case 8: // ColorGoldenRod
                name = tr("Goldenrod");
                break;
            case 9: // ColorOrange
                name = tr("Orange");
                break;
            case 10: // ColorDeepPink
                name = tr("Deep Pink");
                break;
            case 11: // ColorViolet
                name = tr("Violet");
                break;
            case 12: // ColorDarkViolet
                name = tr("Dark Violet");
                break;
            case 13: // ColorMediumSeaGreen
                name = tr("Medium Sea Green");
                break;
            case 14: // ColorLime
                name = tr("Lime");
                break;
            case 15: // ColorDeepSkyBlue
                name = tr("Deep Sky Blue");
                break;
            case 16: // ColorCornFlowerBlue
                name = tr("Corn Flower Blue");
                break;
            case 0: // ColorBlack
            default:
                name = tr("Black");
                break;
        }

        map.insert(colorNames.at(i), name);
    }
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::supportColorsList()
{
    QMap<QString, QString> map;
    map.insert("gold", tr("Gold"));
    map.insert("forestgreen", tr("Forest Green"));
    map.insert("lawngreen", tr("Lawn Green"));
    map.insert("limegreen", tr("Lime Green"));
    map.insert("greenyellow", tr("Green Yellow"));
    map.insert("sandybrown", tr("Sandy Brown"));
    map.insert("orangered", tr("Orange Red"));
    map.insert("maroon", tr("Maroon"));
    map.insert("pink", tr("Pink"));
    map.insert("hotpink", tr("Hot Pink"));
    map.insert("blueviolet", tr("Blue Violet"));
    map.insert("mediumvioletred", tr("Medium Violet Red"));
    map.insert("indigo", tr("Indigo"));
    map.insert("purple", tr("Purple"));
    map.insert("plum", tr("Plum"));
    map.insert("turquoise", tr("Turquoise"));
    map.insert("mediumturquoise", tr("Medium Turquoise"));
    map.insert("powderblue", tr("Powder Blue"));
    map.insert("lightskyblue", tr("Light Sky Blue"));
    map.insert("navy", tr("Navy"));
    map.insert("magenta", tr("Magenta"));
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::backgroundColorsList()
{
    QMap<QString, QString> map;

    map.insert("darkslategrey", tr("Dark Slate Grey"));
    map.insert("grey", tr("Grey"));
    map.insert("gainsboro", tr("Gainsboro"));
    map.insert("darkseagreen", tr("Dark Sea Green"));
    map.insert("lightgrey", tr("Light Grey"));
    map.insert("darkslategrey", tr("Dark Slate Grey"));
    map.insert("lightsteelblue", tr("Light Steel Blue"));
    map.insert("biege", tr("Biege"));
    map.insert("thistle", tr("Thistle"));
    map.insert("silver", tr("Silver"));
    map.insert("whitesmoke", tr("White Smoke"));
    map.insert("white", tr("White"));
    map.insert("darkgrey", tr("Dark Grey"));
    map.insert("cadetblue", tr("Cadet Blue"));
    map.insert("darkkhaki", tr("Dark Khaki"));
    map.insert("tan", tr("Tan"));

    return map;
}

QPixmap VAbstractTool::createColorIcon(const int w, const int h, const QString &color)
{
    QPixmap pixmap(w, h);
    pixmap.fill(QColor(Qt::black));

    QPainter painter(&pixmap);
    painter.setPen(Qt::black);

    const QRect rectangle = QRect(1, 1, w-2, h-2);

    qDebug()<<"createColorIcon - color = "<< color;

    if (color == "No Group")
    {
        painter.fillRect(rectangle, QColor(Qt::white));
        painter.drawLine(0, 0, w, h);
        painter.drawLine(0, h, w, 0);
    }
    else if (color == "By Group")
    {
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        painter.fillRect(rectangle, QColor(Qt::white));
        painter.drawText(rectangle, Qt::AlignCenter, "GROUP");
    }
    else
    {
        painter.fillRect(rectangle, QColor(color));
    }

    return pixmap;
}

//---------------------------------------------------------------------------------------------------------------------
const QStringList VAbstractTool::fills()
{
    const QStringList fills = QStringList() << FillNone              << FillSolid            << FillDense1
                                            << FillDense2            << FillDense3           << FillDense4
                                            << FillDense5            << FillDense6           << FillDense7
                                            << FillHorizLines        << FillVertLines        << FillCross
                                            << FillBackwardDiagonal  << FillForwardDiagonal  << FilldDiagonalCross;
    return fills;
}

//---------------------------------------------------------------------------------------------------------------------
// cppcheck-suppress unusedFunction
QMap<QString, quint32> VAbstractTool::PointsList() const
{
    const QHash<quint32, QSharedPointer<VGObject> > *objs = data.DataGObjects();
    QMap<QString, quint32> list;
    QHash<quint32, QSharedPointer<VGObject> >::const_iterator i;
    for (i = objs->constBegin(); i != objs->constEnd(); ++i)
    {
        if (i.key() != m_id)
        {
            QSharedPointer<VGObject> obj = i.value();
            if (obj->getType() == GOType::Point && obj->getMode() == Draw::Calculation)
            {
                const QSharedPointer<VPointF> point = data.GeometricObject<VPointF>(i.key());
                list[point->name()] = i.key();
            }
        }
    }
    return list;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::setPointNamePosition(quint32 id, const QPointF &pos)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::setPointNameVisiblity(quint32 id, bool visible)
{
    Q_UNUSED(id)
    Q_UNUSED(visible)
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::ToolSelectionType(const SelectionType &type)
{
    selectionType = type;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::RefreshDataInFile()
{
    // do nothing
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::ToolCreation(const Source &typeCreation)
{
    if (typeCreation == Source::FromGui)
    {
        AddToFile();
    }
    else
    {
        RefreshDataInFile();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief AddRecord add record about tool in history.
 * @param id object id in container
 * @param toolType tool type
 * @param doc dom document container
 */
void VAbstractTool::AddRecord(const quint32 id, const Tool &toolType, VAbstractPattern *doc)
{
    QVector<VToolRecord> *history = doc->getHistory();
    VToolRecord record = VToolRecord(id, toolType, doc->getActiveDraftBlockName());
    if (history->contains(record))
    {
        return;
    }

    quint32 cursor = doc->getCursor();
    if (cursor == NULL_ID)
    {
        history->append(record);
    }
    else
    {
        qint32 index = 0;
        for (qint32 i = 0; i<history->size(); ++i)
        {
            VToolRecord rec = history->at(i);
            if (rec.getId() == cursor)
            {
                index = i;
                break;
            }
        }
        history->insert(index+1, record);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::AddNodes(VAbstractPattern *doc, QDomElement &domElement, const VPiecePath &path)
{
    if (path.CountNodes() > 0)
    {
        QDomElement nodesElement = doc->createElement(VAbstractPattern::TagNodes);
        for (int i = 0; i < path.CountNodes(); ++i)
        {
            AddNode(doc, nodesElement, path.at(i));
        }
        domElement.appendChild(nodesElement);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::AddNodes(VAbstractPattern *doc, QDomElement &domElement, const VPiece &piece)
{
    AddNodes(doc, domElement, piece.GetPath());
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractTool::AddSANode(VAbstractPattern *doc, const QString &tagName, const VPieceNode &node)
{
    QDomElement nod = doc->createElement(tagName);

    doc->SetAttribute(nod, AttrIdObject, node.GetId());

    const Tool type = node.GetTypeTool();
    if (type != Tool::NodePoint)
    {
        doc->SetAttribute(nod, VAbstractPattern::AttrNodeReverse, static_cast<quint8>(node.GetReverse()));
    }
    else
    {
        if (node.GetFormulaSABefore() != currentSeamAllowance)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrSABefore, node.GetFormulaSABefore());
        }

        if (node.GetFormulaSAAfter() != currentSeamAllowance)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrSAAfter, node.GetFormulaSAAfter());
        }
    }

    {
        const bool excluded = node.isExcluded();
        if (excluded)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeExcluded, excluded);
        }
        else
        { // For backward compatebility.
            nod.removeAttribute(VAbstractPattern::AttrNodeExcluded);
        }
    }

    switch (type)
    {
        case (Tool::NodeArc):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeArc);
            break;
        case (Tool::NodeElArc):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeElArc);
            break;
        case (Tool::NodePoint):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodePoint);
            break;
        case (Tool::NodeSpline):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeSpline);
            break;
        case (Tool::NodeSplinePath):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeSplinePath);
            break;
        default:
            qDebug()<<"May be wrong tool type!!! Ignoring."<<Q_FUNC_INFO;
            break;
    }

    {
        const unsigned char angleType = static_cast<unsigned char>(node.GetAngleType());

        if (angleType > 0)
        {
            doc->SetAttribute(nod, AttrAngle, angleType);
        }
    }

    if (type == Tool::NodePoint)
    {
        if (node.isNotch())
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeIsNotch,         node.isNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeShowNotch,       node.showNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeShowSecondNotch, node.showSeamlineNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchType,    notchTypeToString(node.getNotchType()));
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchSubType, notchSubTypeToString(node.getNotchSubType()));
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchLength,     node.getNotchLength());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchWidth,      node.getNotchWidth());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchAngle,      node.getNotchAngle());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchCount,      node.getNotchCount());
        }

        if (not node.isNotch() && node.getNotchType() == NotchType::Slit
            && node.getNotchSubType() == NotchSubType::Straightforward)
        { // For backward compatebility.
            nod.removeAttribute(VAbstractPattern::AttrNodeIsNotch);
            nod.removeAttribute(VAbstractPattern::AttrNodeNotchType);
            nod.removeAttribute(VAbstractPattern::AttrNodeNotchSubType);
        }
    }
    else
    { // Wrong configuration.
        nod.removeAttribute(VAbstractPattern::AttrNodeIsNotch);
        nod.removeAttribute(VAbstractPattern::AttrNodeNotchType);
        nod.removeAttribute(VAbstractPattern::AttrNodeNotchSubType);
    }

    return nod;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::AddNode(VAbstractPattern *doc, QDomElement &domElement, const VPieceNode &node)
{
    domElement.appendChild(AddSANode(doc, VAbstractPattern::TagNode, node));
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VPieceNode> VAbstractTool::PrepareNodes(const VPiecePath &path, VMainGraphicsScene *scene,
                                                VAbstractPattern *doc, VContainer *data)
{
    QVector<VPieceNode> nodes;
    for (int i = 0; i< path.CountNodes(); ++i)
    {
        VPieceNode node = path.at(i);
        const quint32 id = PrepareNode(node, scene, doc, data);
        if (id > NULL_ID)
        {
            node.SetId(id);
            nodes.append(node);
        }
    }
    return nodes;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VAbstractTool::PrepareNode(const VPieceNode &node, VMainGraphicsScene *scene,
                                   VAbstractPattern *doc, VContainer *data)
{
    SCASSERT(scene != nullptr)
    SCASSERT(doc != nullptr)
    SCASSERT(data != nullptr)

    quint32 id = NULL_ID;
    switch (node.GetTypeTool())
    {
        case (Tool::NodePoint):
            id = CreateNode<VPointF>(data, node.GetId());
            VNodePoint::Create(doc, data, scene, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeArc):
            id = CreateNode<VArc>(data, node.GetId());
            VNodeArc::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeElArc):
            id = CreateNode<VEllipticalArc>(data, node.GetId());
            VNodeEllipticalArc::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeSpline):
            id = CreateNodeSpline(data, node.GetId());
            VNodeSpline::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeSplinePath):
            id = CreateNodeSplinePath(data, node.GetId());
            VNodeSplinePath::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        default:
            qDebug()<<"May be wrong tool type!!! Ignoring."<<Q_FUNC_INFO;
            break;
    }
    return id;
}
