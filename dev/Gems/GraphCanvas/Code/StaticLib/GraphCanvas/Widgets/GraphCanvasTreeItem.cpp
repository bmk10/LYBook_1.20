/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <AzCore/std/functional_basic.h>

#include <GraphCanvas/Widgets/GraphCanvasTreeItem.h>
#include <GraphCanvas/Widgets/GraphCanvasTreeModel.h>

namespace GraphCanvas
{
    ////////////////////////
    // GraphCanvasTreeItem
    ////////////////////////
    GraphCanvasTreeItem::GraphCanvasTreeItem()
        : m_abstractItemModel(nullptr)
        , m_allowSignals(true)
        , m_deleteRemoveChildren(false)
        , m_allowPruneOnEmpty(true)
        , m_parent(nullptr)
    {
    }
    
    GraphCanvasTreeItem::~GraphCanvasTreeItem()
    {
        if (m_parent)
        {
            const bool deleteObject = false;
            m_parent->RemoveChild(this, deleteObject);
        }

        for (GraphCanvasTreeItem* item : m_childItems)
        {
            item->RemoveParent(this);
            delete item;
        }

        m_childItems.clear();
    }

    void GraphCanvasTreeItem::SetAllowPruneOnEmpty(bool allowsEmpty)
    {
        m_allowPruneOnEmpty = allowsEmpty;
    }

    bool GraphCanvasTreeItem::AllowPruneOnEmpty() const
    {
        return m_allowPruneOnEmpty;
    }

    void GraphCanvasTreeItem::DetachItem()
    {
        if (m_parent)
        {
            const bool deleteObject = false;
            m_parent->RemoveChild(this, deleteObject);
            ClearModel();
        }
    }
    
    int GraphCanvasTreeItem::GetChildCount() const
    {
        return static_cast<int>(m_childItems.size());
    }

    void GraphCanvasTreeItem::ClearChildren()
    {
        if (m_abstractItemModel)
        {
            m_deleteRemoveChildren = true;
            m_abstractItemModel->removeRows(0, GetChildCount(), m_abstractItemModel->CreateIndex(this));
            m_deleteRemoveChildren = false;
        }
        else
        {
            while (!m_childItems.empty())
            {
                GraphCanvasTreeItem* item = m_childItems.back();
                m_childItems.pop_back();

                delete item;
            }
        }
    }

    GraphCanvasTreeItem* GraphCanvasTreeItem::FindChildByRow(int row) const
    {
        GraphCanvasTreeItem* treeItem = nullptr;

        if (row >= 0 && row < m_childItems.size())
        {
            treeItem = m_childItems[row];
        }

        return treeItem;
    }

    int GraphCanvasTreeItem::FindRowUnderParent() const
    {
        if (m_parent)
        {
            return m_parent->FindRowForChild(this);
        }
        else
        {
            return 0;
        }
    }

    GraphCanvasTreeItem* GraphCanvasTreeItem::GetParent() const
    {
        return m_parent;
    }

    void GraphCanvasTreeItem::RegisterModel(GraphCanvasTreeModel* itemModel)
    {
        AZ_Assert(m_abstractItemModel == nullptr || m_abstractItemModel == itemModel, "GraphCanvasTreeItem being registered to two models at the same time.");

        if (m_abstractItemModel == nullptr)
        {
            m_abstractItemModel = itemModel;

            for (auto treeItem : m_childItems)
            {
                treeItem->RegisterModel(itemModel);
            }
        }
    }

    int GraphCanvasTreeItem::FindRowForChild(const GraphCanvasTreeItem* item) const
    {
        int row = -1;

        for (int i = 0; i < static_cast<int>(m_childItems.size()); ++i)
        {
            if (m_childItems[i] == item)
            {
                row = i;
                break;
            }
        }

        AZ_Warning("GraphCanvasTreeItem", row >= 0, "Could not find item in it's parent.");

        return row;
    }

    void GraphCanvasTreeItem::RemoveParent(GraphCanvasTreeItem* item)
    {
        AZ_Warning("GraphCanvasTreeItem", m_parent == item, "Trying to remove node from an unknown parent.");
        if (m_parent == item)
        {
            m_parent = nullptr;
            ClearModel();
        }
    }	

    void GraphCanvasTreeItem::AddChild(GraphCanvasTreeItem* item, bool signalAdd)
    {
        static const Comparator k_insertionComparator = {};

        if (item->m_parent == this)
        {
            return;
        }

        if (item->m_parent != nullptr)
        {
            item->m_parent->RemoveChild(item);
        }

        if (m_abstractItemModel)
        {
            item->RegisterModel(m_abstractItemModel);
        }

        PreOnChildAdded(item);

        AZStd::vector< GraphCanvasTreeItem* >::iterator insertPoint = AZStd::lower_bound(m_childItems.begin(), m_childItems.end(), item, k_insertionComparator);

        int rowNumber = AZStd::distance(m_childItems.begin(), insertPoint);

        if (m_abstractItemModel && signalAdd)
        {
            // Optimization TODO: Provide some method to batch all of these adds
            m_abstractItemModel->ChildAboutToBeAdded(this, rowNumber);
        }

        m_childItems.insert(insertPoint, item);

        item->m_parent = this;

        OnChildAdded(item);

        if (m_abstractItemModel && signalAdd)
        {
            m_abstractItemModel->OnChildAdded();
        }
    }

    void GraphCanvasTreeItem::RemoveChild(GraphCanvasTreeItem* item, bool deleteObject)
    {
        m_deleteRemoveChildren = deleteObject;

        if (item->m_parent == this)
        {
            // Remove child cannot rely on the comparator being valid.
            // Since the default comparator just returns false.
            for (int i = 0; i < GetChildCount(); ++i)
            {
                GraphCanvasTreeItem* currentItem = m_childItems[i];
                if (currentItem == item)
                {
                    if (m_abstractItemModel)
                    {
                        m_abstractItemModel->removeRows(i, 1, m_abstractItemModel->CreateIndex(this));
                    }
                    else
                    {
                        GraphCanvasTreeItem* treeItem = m_childItems[i];
                        m_childItems.erase(m_childItems.begin() + i);

                        if (deleteObject)
                        {
                            delete treeItem;
                        }
                    }

                    
                    break;
                }
            }
        }

        m_deleteRemoveChildren = false;
    }

    void GraphCanvasTreeItem::SignalLayoutAboutToBeChanged()
    {
        if (m_abstractItemModel && m_allowSignals)
        {
            m_abstractItemModel->layoutAboutToBeChanged();
        }
    }

    void GraphCanvasTreeItem::SignalLayoutChanged()
    {
        if (m_abstractItemModel && m_allowSignals)
        {
            m_abstractItemModel->layoutChanged();
        }
    }

    void GraphCanvasTreeItem::SignalDataChanged()
    {
        if (m_abstractItemModel)
        {
            m_abstractItemModel->dataChanged(m_abstractItemModel->CreateIndex(this), m_abstractItemModel->CreateIndex(this, GetColumnCount() - 1));

            if (m_parent)
            {
                m_parent->OnChildDataChanged(this);
            }
        }
    }
    
    bool GraphCanvasTreeItem::LessThan(const GraphCanvasTreeItem* graphItem) const
    {
        return true;
    }

    void GraphCanvasTreeItem::PreOnChildAdded(GraphCanvasTreeItem* item)
    {
        AZ_UNUSED(item);
    }

    void GraphCanvasTreeItem::OnChildAdded(GraphCanvasTreeItem* treeItem)
    {
        AZ_UNUSED(treeItem);
    }

    void GraphCanvasTreeItem::OnChildDataChanged(GraphCanvasTreeItem* item)
    {
        AZ_UNUSED(item);
    }

    void GraphCanvasTreeItem::ClearModel()
    {
        m_abstractItemModel = nullptr;

        for (GraphCanvasTreeItem* item : m_childItems)
        {
            item->ClearModel();
        }
    }
};