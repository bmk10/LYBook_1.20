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

#include <AzCore/Asset/AssetTypeInfoBus.h>
#include <AzCore/std/containers/vector.h>

#include <AzToolsFramework/AssetBrowser/Search/SearchAssetTypeSelectorWidget.h>
#include <AzToolsFramework/AssetBrowser/Search/Filter.h>
#include <AzToolsFramework/AssetBrowser/Search/FilterByWidget.h>

#include <AssetBrowser/Search/ui_SearchAssetTypeSelectorWidget.h>

#include <QPushButton>
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <algorithm>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        SearchAssetTypeSelectorWidget::SearchAssetTypeSelectorWidget(QWidget* parent)
            : QWidget(parent)
            , m_ui(new Ui::SearchAssetTypeSelectorWidgetClass())
            , m_filter(QSharedPointer<CompositeFilter>(new CompositeFilter(CompositeFilter::LogicOperatorType::OR)))
            , m_locked(false)
        {
            m_ui->setupUi(this);

            QMenu* menu = new QMenu(this);
            AddAllAction(menu);
            menu->addSeparator();

            EBusAggregateUniqueResults<QString> results;
            AZ::AssetTypeInfoBus::BroadcastResult(results, &AZ::AssetTypeInfo::GetGroup);
            std::sort(results.values.begin(), results.values.end(),
                [](const QString& a, const QString& b) { return QString::compare(a, b, Qt::CaseInsensitive) < 0; });

            for (QString& group : results.values)
            {
                // Group "Other" should be in the end of the list, and "Hidden" should not be on the list at all
                if (group == "Other" || group == "Hidden")
                {
                    continue;
                }
                AddAssetTypeGroup(menu, group);
            }
            AddAssetTypeGroup(menu, "Other");
            menu->setLayoutDirection(Qt::LeftToRight);
            menu->setStyleSheet("border: none; background-color: #333333;");
            m_ui->m_showSelectionButton->setMenu(menu);

            m_filter->SetTag("AssetTypes");
            m_filter->SetFilterPropagation(AssetBrowserEntryFilter::PropagateDirection::Down);
        }

        SearchAssetTypeSelectorWidget::~SearchAssetTypeSelectorWidget()
        {
        }

        void SearchAssetTypeSelectorWidget::UpdateFilterByWidget() const
        {
            for (auto assetTypeCheckbox : m_assetTypeCheckboxes)
            {
                if (assetTypeCheckbox->isChecked())
                {
                    m_filterByWidget->ToggleClearButton(true);
                    return;
                }
            }
            m_filterByWidget->ToggleClearButton(false);
        }

        void SearchAssetTypeSelectorWidget::ClearAll() const
        {
            // check all other asset types
            for (auto assetTypeCheckbox : m_assetTypeCheckboxes)
            {
                if (assetTypeCheckbox->isChecked())
                {
                    assetTypeCheckbox->setChecked(false);
                }
            }
            m_filter->RemoveAllFilters();
            m_filter->SetEmptyResult(true);
            UpdateFilterByWidget();
        }

        FilterConstType SearchAssetTypeSelectorWidget::GetFilter() const
        {
            return m_filter;
        }

        bool SearchAssetTypeSelectorWidget::IsLocked() const
        {
            return m_locked;
        }

        void SearchAssetTypeSelectorWidget::AddAssetTypeGroup(QMenu* menu, const QString& group)
        {
            EBusAggregateAssetTypesIfBelongsToGroup results(group);
            AZ::AssetTypeInfoBus::BroadcastResult(results, &AZ::AssetTypeInfo::GetAssetType);

            if (!results.values.empty())
            {
                QCheckBox* checkbox = new QCheckBox(group, menu);
                QWidgetAction* action = new QWidgetAction(menu);
                action->setDefaultWidget(checkbox);
                menu->addAction(action);
                m_assetTypeCheckboxes.push_back(checkbox);

                AssetGroupFilter* groupFilter = new AssetGroupFilter();
                groupFilter->SetAssetGroup(group);

                m_actionFiltersMapping[checkbox] = FilterConstType(groupFilter);

                connect(checkbox, &QCheckBox::clicked, this,
                    [=](bool checked)
                    {
                        if (checked)
                        {
                            m_filter->AddFilter(m_actionFiltersMapping[checkbox]);
                        }
                        else
                        {
                            m_filter->RemoveFilter(m_actionFiltersMapping[checkbox]);
                        }
                        UpdateFilterByWidget();
                    });
            }
        }

        void SearchAssetTypeSelectorWidget::AddAllAction(QMenu* menu)
        {
            m_filterByWidget = new FilterByWidget(menu);
            auto action = new QWidgetAction(menu);
            action->setDefaultWidget(m_filterByWidget);
            menu->addAction(action);
            connect(m_filterByWidget, &FilterByWidget::ClearSignal, this, &SearchAssetTypeSelectorWidget::ClearAll);
        }
    } // namespace AssetBrowser
} // namespace AzToolsFramework
#include <AssetBrowser/Search/SearchAssetTypeSelectorWidget.moc>