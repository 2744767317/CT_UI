#pragma once

#include "markupsnode.h"

#include <QString>
#include <QVariantList>
#include <QVector3D>

#include <optional>
#include <vector>

enum class MarkupsTool {
    None = 0,
    Mark = 1,
    Length = 2,
    Angle = 3,
    Perimeter = 4
};

class MarkupsScene
{
public:
    MarkupsTool tool() const { return m_tool; }
    void setTool(MarkupsTool tool);

    bool visible() const { return m_visible; }
    void setVisible(bool visible);

    int markCount() const;
    int measureCount() const;

    int revision() const { return m_revision; }
    bool hasActive() const { return m_active.has_value(); }

    const std::vector<MarkupsNode> &nodes() const { return m_nodes; }
    const std::optional<MarkupsNode> &activeNode() const { return m_active; }

    QString activeLabelPreview() const;
    QVariantList itemsVariant() const;
    QVariantList activePointsVariant() const;
    QVariantList renderItemsVariant() const;

    bool addWorldPoint(const QVector3D &world);
    bool finishActive();
    void cancelActive();
    void clearAll();

    void setNodeVisible(int nodeId, bool visible);
    void setNodeColor(int nodeId, const QString &color);
    bool removeNode(int nodeId);

    /// 拖拽编辑：更新已提交节点的控制点（世界坐标）。
    bool updateControlPoint(int nodeId, int pointIndex, const QVector3D &world);

    MarkupsNode *findNode(int nodeId);
    const MarkupsNode *findNode(int nodeId) const;

private:
    void bump();
    QString nextLabel(MarkupsNodeType type);
    bool tryCommitActive();

    MarkupsTool m_tool = MarkupsTool::None;
    bool m_visible = true;
    int m_revision = 0;
    int m_nextId = 1;
    int m_nextPoint = 1;
    int m_nextLength = 1;
    int m_nextAngle = 1;
    int m_nextCurve = 1;
    std::vector<MarkupsNode> m_nodes;
    std::optional<MarkupsNode> m_active;
};
