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

#pragma once

#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/Node.h>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <Editor/ActorEditorBus.h>
#include <Editor/QtMetaTypes.h>
#include <QIcon>


namespace EMotionFX
{
    class Skeleton;

    // Skeleton model
    // Columns: Node Name
    class SkeletonModel
        : public QAbstractItemModel
        , private EMotionFX::ActorEditorNotificationBus::Handler
    {
        Q_OBJECT // AUTOMOC

    public:
        enum ColumnIndex
        {
            COLUMN_NAME,
            COLUMN_RAGDOLL_LIMIT,
            COLUMN_RAGDOLL_COLLIDERS,
            COLUMN_HITDETECTION_COLLIDERS,
            COLUMN_CLOTH_COLLIDERS
        };

        enum Role
        {
            ROLE_NODE_INDEX = Qt::UserRole,
            ROLE_POINTER,
            ROLE_ACTOR_POINTER,
            ROLE_ACTOR_INSTANCE_POINTER,
            ROLE_BONE,
            ROLE_HASMESH,
            ROLE_RAGDOLL,
            ROLE_HITDETECTION,
            ROLE_CLOTH,
        };

        SkeletonModel();
        ~SkeletonModel() override;

        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex& child) const override;

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        Qt::ItemFlags flags(const QModelIndex& index) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;

        QModelIndex GetModelIndex(Node* node) const;
        QModelIndexList GetModelIndicesForFullSkeleton() const;

        QItemSelectionModel& GetSelectionModel() { return m_selectionModel; }

        void SetCheckable(bool isCheckable);

        void ForEach(const AZStd::function<void(const QModelIndex& modelIndex)>& func);

        // ActorEditorNotificationBus overrides
        void ActorSelectionChanged(Actor* actor) override;
        void ActorInstanceSelectionChanged(EMotionFX::ActorInstance* actorInstance) override;

        Skeleton* GetSkeleton() const                           { return m_skeleton; }
        Actor* GetActor() const                                 { return m_actor; }
        ActorInstance* GetActorInstance() const                 { return m_actorInstance; }

        static int s_defaultIconSize;

    private:
        struct NodeInfo
        {
            bool m_hasMesh = false;
            bool m_isBone = false;
            bool m_checkable = false;
            Qt::CheckState m_checkState = Qt::Unchecked;
        };

        void SetActor(Actor* actor);
        void SetActorInstance(ActorInstance* actorInstance);
        void UpdateNodeInfos(Actor* actor);
        void Reset();

        static int s_columnCount;

        AZStd::vector<NodeInfo> m_nodeInfos;
        Skeleton* m_skeleton;
        Actor* m_actor;
        ActorInstance* m_actorInstance;
        QItemSelectionModel m_selectionModel;

        QIcon m_jointIcon;
        QIcon m_clothColliderIcon;
        QIcon m_hitDetectionColliderIcon;
        QIcon m_ragdollColliderIcon;
        QIcon m_ragdollJointLimitIcon;
    };

} // namespace EMotionFX