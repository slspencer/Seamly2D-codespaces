/***************************************************************************
 **  @file   remove_groupitem.cpp
 **  @author Douglas S Caskey
 **  @date   Mar 1, 2023
 **
 **  @copyright
 **  Copyright (C) 2023 Seamly, LLC
 **  https://github.com/fashionfreedom/seamly2d
 **
 **  @brief
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
 **  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
 **************************************************************************/

#include "remove_groupitem.h"

#include <QDomNode>
#include <QDomNodeList>

#include "../vmisc/logging.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/def.h"
#include "../vwidgets/vmaingraphicsview.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../vtools/tools/vdatatool.h"
#include "vundocommand.h"

//---------------------------------------------------------------------------------------------------------------------

RemoveGroupItem::RemoveGroupItem(const QDomElement &xml, VAbstractPattern *doc, quint32 groupId, QUndoCommand *parent)
    : VUndoCommand(xml, doc, parent)
    , m_activeDrawName(doc->getActiveDraftBlockName())
{
    setText(tr("Delete group item"));
    nodeId = groupId;
}

//---------------------------------------------------------------------------------------------------------------------
RemoveGroupItem::~RemoveGroupItem()
{
}

//---------------------------------------------------------------------------------------------------------------------
void RemoveGroupItem::undo()
{
    qCDebug(vUndo, "Undo delete group item");
    doc->changeActiveDraftBlock(m_activeDrawName);//Without this user will not see this change

    QDomElement group = doc->elementById(nodeId, VAbstractPattern::TagGroup);
    if (group.isElement())
    {
        if (group.appendChild(xml).isNull())
        {
            qCDebug(vUndo, "Can't add the item.");
            return;
        }

        doc->SetModified(false);
        emit qApp->getCurrentDocument()->patternChanged(true);

        QDomElement groups = doc->createGroups();
        if (not groups.isNull())
        {
            doc->parseGroups(groups);
        } else
        {
            qCDebug(vUndo, "Can't get tag Groups.");
            return;
        }

        emit updateGroups();
    }
    else
    {
        qCDebug(vUndo, "Can't get group by id = %u.", nodeId);
        return;
    }

    VMainGraphicsView::NewSceneRect(qApp->getCurrentScene(), qApp->getSceneView());
    emit doc->setCurrentDraftBlock(m_activeDrawName); //Return current draft Block after undo
}

//---------------------------------------------------------------------------------------------------------------------
void RemoveGroupItem::redo()
{
    qCDebug(vUndo, "Redo add group item");
    doc->changeActiveDraftBlock(m_activeDrawName);//Without this user will not see this change

    QDomElement group = doc->elementById(nodeId, VAbstractPattern::TagGroup);
    if (group.isElement())
    {
        if (group.removeChild(xml).isNull())
        {
            qCDebug(vUndo, "Can't delete item.");
            return;
        }

        doc->SetModified(true);
        emit qApp->getCurrentDocument()->patternChanged(false);

        // set the item visible. Because if the undo is done when unvisibile and it's not in any group after the
        // undo, it stays unvisible until the entire drawing is completly rerendered.
        quint32 objectId = doc->GetParametrUInt(xml,QString("object"),NULL_ID_STR);
        quint32 toolId = doc->GetParametrUInt(xml,QString("tool"),NULL_ID_STR);
        VDataTool* tool = doc->getTool(toolId);
        tool->GroupVisibility(objectId,true);

        QDomElement groups = doc->createGroups();
        if (not groups.isNull())
        {
            doc->parseGroups(groups);
        } else
        {
            qCDebug(vUndo, "Can't get tag Groups.");
            return;
        }

        emit updateGroups();
    }
    else
    {
        qCDebug(vUndo, "Can't get group by id = %u.", nodeId);
        return;
    }

    VMainGraphicsView::NewSceneRect(qApp->getCurrentScene(), qApp->getSceneView());
    emit doc->setCurrentDraftBlock(m_activeDrawName); //Return current draft Block after undo
}
