#include "markupsscene.h"

#include "markupsmetrics.h"

#include <algorithm>
#include <cmath>

namespace {

QVariantMap pointVariant(const QVector3D &p)
{
    QVariantMap map;
    map.insert(QStringLiteral("x"), p.x());
    map.insert(QStringLiteral("y"), p.y());
    map.insert(QStringLiteral("z"), p.z());
    return map;
}

QVariantMap nodeVariant(const MarkupsNode &node)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), node.id);
    map.insert(QStringLiteral("type"), static_cast<int>(node.type));
    map.insert(QStringLiteral("label"), node.label);
    map.insert(QStringLiteral("displayText"), node.displayText);
    map.insert(QStringLiteral("color"), node.color);
    map.insert(QStringLiteral("viewId"), node.viewId);
    map.insert(QStringLiteral("visible"), node.visible);
    map.insert(QStringLiteral("closed"), node.closed);
    map.insert(QStringLiteral("metric"), node.metric);
    QVariantList points;
    for (const QVector3D &point : node.controlPoints)
        points.push_back(pointVariant(point));
    map.insert(QStringLiteral("points"), points);
    // 兼容旧 renderItems：同时提供体素风格字段时由上层转换；此处给世界坐标。
    map.insert(QStringLiteral("worldPoints"), points);
    return map;
}

MarkupsNodeType typeForTool(MarkupsTool tool)
{
    switch (tool) {
    case MarkupsTool::Mark:
        return MarkupsNodeType::Point;
    case MarkupsTool::Length:
        return MarkupsNodeType::Line;
    case MarkupsTool::Angle:
        return MarkupsNodeType::Angle;
    case MarkupsTool::Perimeter:
        return MarkupsNodeType::ClosedCurve;
    case MarkupsTool::None:
        break;
    }
    return MarkupsNodeType::Point;
}

int requiredPoints(MarkupsNodeType type)
{
    switch (type) {
    case MarkupsNodeType::Point:
        return 1;
    case MarkupsNodeType::Line:
        return 2;
    case MarkupsNodeType::Angle:
        return 3;
    case MarkupsNodeType::ClosedCurve:
        return 3;
    }
    return 1;
}

} // namespace

void MarkupsScene::setTool(MarkupsTool tool)
{
    if (m_tool == tool)
        return;
    if (m_active) {
        const bool persistentList = m_active->type == MarkupsNodeType::Point;
        const bool curve = m_active->type == MarkupsNodeType::ClosedCurve;
        const std::size_t minimumPoints = persistentList ? 1u : 2u;
        if ((persistentList || curve) && m_active->controlPoints.size() >= minimumPoints)
            finishActive();
        else
            cancelActive();
    }
    m_tool = tool;
    bump();
}

void MarkupsScene::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    bump();
}

void MarkupsScene::bump()
{
    ++m_revision;
}

QString MarkupsScene::nextLabel(MarkupsNodeType type)
{
    switch (type) {
    case MarkupsNodeType::Point:
        return QStringLiteral("P%1").arg(m_nextPoint++);
    case MarkupsNodeType::Line:
        return QStringLiteral("L%1").arg(m_nextLength++);
    case MarkupsNodeType::Angle:
        return QStringLiteral("A%1").arg(m_nextAngle++);
    case MarkupsNodeType::ClosedCurve:
        return QStringLiteral("C%1").arg(m_nextCurve++);
    }
    return QStringLiteral("X");
}

QString MarkupsScene::activeLabelPreview() const
{
    switch (m_tool) {
    case MarkupsTool::Mark:
        return QStringLiteral("P%1").arg(m_nextPoint);
    case MarkupsTool::Length:
        return QStringLiteral("L%1").arg(m_nextLength);
    case MarkupsTool::Angle:
        return QStringLiteral("A%1").arg(m_nextAngle);
    case MarkupsTool::Perimeter:
        return QStringLiteral("C%1").arg(m_nextCurve);
    case MarkupsTool::None:
        return {};
    }
    return {};
}

QVariantList MarkupsScene::itemsVariant() const
{
    QVariantList list;
    for (const MarkupsNode &node : m_nodes)
        list.push_back(nodeVariant(node));
    return list;
}

QVariantList MarkupsScene::activePointsVariant() const
{
    QVariantList list;
    if (!m_active)
        return list;
    for (const QVector3D &point : m_active->controlPoints)
        list.push_back(pointVariant(point));
    return list;
}

QVariantList MarkupsScene::renderItemsVariant() const
{
    if (!m_visible)
        return {};
    QVariantList list = itemsVariant();
    if (m_active && !m_active->controlPoints.empty()) {
        MarkupsNode preview = *m_active;
        if (preview.label.isEmpty())
            preview.label = activeLabelPreview();
        refreshMarkupsMetrics(&preview);
        list.push_back(nodeVariant(preview));
    }
    return list;
}

bool MarkupsScene::restoreItems(const QVariantList &items)
{
    std::vector<MarkupsNode> restored;
    restored.reserve(static_cast<std::size_t>(items.size()));
    int nextId = 1;

    for (const QVariant &entry : items) {
        const QVariantMap map = entry.toMap();
        bool typeOk = false;
        const int rawType = map.value(QStringLiteral("type")).toInt(&typeOk);
        if (!typeOk || rawType < static_cast<int>(MarkupsNodeType::Point)
            || rawType > static_cast<int>(MarkupsNodeType::ClosedCurve))
            return false;

        const QVariantList pointValues = map.value(QStringLiteral("points")).toList();
        const int required = requiredPoints(static_cast<MarkupsNodeType>(rawType));
        if (pointValues.isEmpty()
            || (rawType != static_cast<int>(MarkupsNodeType::ClosedCurve)
                && pointValues.size() < required))
            return false;

        MarkupsNode node;
        node.id = map.value(QStringLiteral("id")).toInt();
        if (node.id <= 0)
            node.id = nextId;
        for (const MarkupsNode &existing : restored) {
            if (existing.id == node.id)
                return false;
        }
        nextId = std::max(nextId, node.id + 1);
        node.type = static_cast<MarkupsNodeType>(rawType);
        node.label = map.value(QStringLiteral("label")).toString();
        node.color = map.value(QStringLiteral("color"), node.color).toString();
        node.viewId = map.value(QStringLiteral("viewId")).toString();
        node.visible = map.value(QStringLiteral("visible"), true).toBool();
        node.closed = map.value(QStringLiteral("closed"), false).toBool();
        for (const QVariant &pointValue : pointValues) {
            const QVariantMap point = pointValue.toMap();
            bool xOk = false;
            bool yOk = false;
            bool zOk = false;
            const float x = point.value(QStringLiteral("x")).toFloat(&xOk);
            const float y = point.value(QStringLiteral("y")).toFloat(&yOk);
            const float z = point.value(QStringLiteral("z")).toFloat(&zOk);
            if (!xOk || !yOk || !zOk || !std::isfinite(x) || !std::isfinite(y)
                || !std::isfinite(z))
                return false;
            node.controlPoints.emplace_back(x, y, z);
        }
        if (node.type == MarkupsNodeType::ClosedCurve && node.controlPoints.size() < 2)
            return false;
        refreshMarkupsMetrics(&node);
        restored.push_back(std::move(node));
    }

    m_nodes = std::move(restored);
    m_active.reset();
    m_nextId = nextId;
    m_nextPoint = 1;
    m_nextLength = 1;
    m_nextAngle = 1;
    m_nextCurve = 1;
    bump();
    return true;
}

bool MarkupsScene::tryCommitActive()
{
    if (!m_active)
        return false;
    const int need = requiredPoints(m_active->type);
    if (m_active->type == MarkupsNodeType::Point
        || m_active->type == MarkupsNodeType::ClosedCurve)
        return false;
    if (static_cast<int>(m_active->controlPoints.size()) < need)
        return false;
    refreshMarkupsMetrics(&(*m_active));
    m_nodes.push_back(*m_active);
    m_active.reset();
    return true;
}

bool MarkupsScene::addWorldPoint(const QVector3D &world, const QString &viewId)
{
    if (m_tool == MarkupsTool::None)
        return false;

    if (!m_active) {
        MarkupsNode node;
        node.id = m_nextId++;
        node.type = typeForTool(m_tool);
        node.label = nextLabel(node.type);
        node.color = QStringLiteral("#E53935");
        node.viewId = viewId;
        m_active = node;
    } else if (m_active->viewId != viewId) {
        // A projection markup belongs to the view where its first point was placed.
        // Do not reinterpret its next control point in the paired projection.
        return false;
    }

    m_active->controlPoints.push_back(world);
    refreshMarkupsMetrics(&(*m_active));

    if (m_active->type == MarkupsNodeType::Point
        || m_active->type == MarkupsNodeType::ClosedCurve) {
        bump();
        return true;
    }
    if (tryCommitActive()) {
        bump();
        return true;
    }
    bump();
    return true;
}

bool MarkupsScene::finishActive()
{
    if (!m_active)
        return false;
    auto &points = m_active->controlPoints;
    if (m_active->type == MarkupsNodeType::Point) {
        if (points.empty())
            return false;
    } else if (m_active->type == MarkupsNodeType::ClosedCurve) {
        if (points.size() < 2)
            return false;
        // Slicer's Curve markup is open by default. Closing is a separate display/node option.
        m_active->closed = false;
    } else {
        return false;
    }
    refreshMarkupsMetrics(&(*m_active));
    m_nodes.push_back(*m_active);
    m_active.reset();
    bump();
    return true;
}

void MarkupsScene::cancelActive()
{
    if (!m_active)
        return;
    m_active.reset();
    bump();
}

void MarkupsScene::clearAll()
{
    const bool had = !m_nodes.empty() || m_active.has_value();
    m_nodes.clear();
    m_active.reset();
    m_nextId = 1;
    m_nextPoint = 1;
    m_nextLength = 1;
    m_nextAngle = 1;
    m_nextCurve = 1;
    if (had)
        bump();
}

void MarkupsScene::setNodeVisible(int nodeId, bool visible)
{
    if (MarkupsNode *node = findNode(nodeId)) {
        if (node->visible != visible) {
            node->visible = visible;
            bump();
        }
    }
}

void MarkupsScene::setNodeColor(int nodeId, const QString &color)
{
    if (MarkupsNode *node = findNode(nodeId)) {
        if (node->color != color) {
            node->color = color;
            bump();
        }
    }
}

bool MarkupsScene::removeNode(int nodeId)
{
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (it->id == nodeId) {
            m_nodes.erase(it);
            bump();
            return true;
        }
    }
    return false;
}

int MarkupsScene::markCount() const
{
    int count = (m_active && m_active->type == MarkupsNodeType::Point) ? 1 : 0;
    for (const MarkupsNode &node : m_nodes)
        if (node.type == MarkupsNodeType::Point)
            ++count;
    return count;
}

int MarkupsScene::measureCount() const
{
    int count = 0;
    for (const MarkupsNode &node : m_nodes)
        if (node.type != MarkupsNodeType::Point)
            ++count;
    return count;
}

bool MarkupsScene::updateControlPoint(int nodeId, int pointIndex, const QVector3D &world)
{
    MarkupsNode *node = findNode(nodeId);
    if (!node || pointIndex < 0
        || pointIndex >= static_cast<int>(node->controlPoints.size()))
        return false;
    node->controlPoints[static_cast<std::size_t>(pointIndex)] = world;
    refreshMarkupsMetrics(node);
    bump();
    return true;
}

MarkupsNode *MarkupsScene::findNode(int nodeId)
{
    for (MarkupsNode &node : m_nodes) {
        if (node.id == nodeId)
            return &node;
    }
    return nullptr;
}

const MarkupsNode *MarkupsScene::findNode(int nodeId) const
{
    for (const MarkupsNode &node : m_nodes) {
        if (node.id == nodeId)
            return &node;
    }
    return nullptr;
}
