#pragma once

#include "src/dicom/medicaldatacontroller.h"
#include "src/markups/markupsscene.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <map>

class AnnotationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int toolType READ toolType WRITE setToolType NOTIFY toolChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool hasActive READ hasActive NOTIFY annotationsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY annotationsChanged)
    Q_PROPERTY(int markCount READ markCount NOTIFY annotationsChanged)
    Q_PROPERTY(int measureCount READ measureCount NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList activePoints READ activePoints NOTIFY annotationsChanged)
    Q_PROPERTY(QString activeLabelPreview READ activeLabelPreview NOTIFY annotationsChanged)

public:
    enum ToolType {
        NoneTool = 0,
        PointListTool = 1,
        MarkTool = PointListTool,
        LineTool = 2,
        LengthTool = LineTool,
        AngleTool = 3,
        CurveTool = 4,
        PerimeterTool = CurveTool
    };
    Q_ENUM(ToolType)

    explicit AnnotationController(QObject *parent = nullptr);

    int toolType() const;
    bool visible() const;
    bool hasActive() const;
    int revision() const;
    int markCount() const;
    int measureCount() const;
    QVariantList items() const;
    QVariantList activePoints() const;
    QString activeLabelPreview() const;

    MedicalDataController *medicalData() const { return m_medicalData; }
    void setMedicalData(MedicalDataController *data);

    const MarkupsScene &scene() const { return m_scene; }
    MarkupsScene &scene() { return m_scene; }

    void setToolType(int type);
    void setVisible(bool visible);

    Q_INVOKABLE bool addControlPoint(int voxelX, int voxelY, int voxelZ);
    Q_INVOKABLE bool addWorldPoint(double x, double y, double z);
    bool addWorldPointForView(double x, double y, double z, const QString &viewId);
    Q_INVOKABLE bool finishActive();
    Q_INVOKABLE void cancelActive();
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE bool updateControlPoint(int nodeId, int pointIndex,
                                        double x, double y, double z);
    Q_INVOKABLE bool updateControlPointFromVoxel(int nodeId, int pointIndex,
                                                 int voxelX, int voxelY, int voxelZ);

    /// 供 VTK Representation 读取（世界坐标点）。
    Q_INVOKABLE QVariantList renderItems() const;

    /// 逐个标注操作（作用于当前活动数据集的 scene）。
    Q_INVOKABLE void setNodeVisible(int nodeId, bool visible);
    Q_INVOKABLE void setNodeColor(int nodeId, const QString &color);
    Q_INVOKABLE bool removeNode(int nodeId);

    /// 任意数据集的计数（供数据集列表行显示）。
    Q_INVOKABLE int markCountFor(const QString &volumeId) const;
    Q_INVOKABLE int measureCountFor(const QString &volumeId) const;

public slots:
    void onMedicalDataChanged();

signals:
    void toolChanged();
    void visibleChanged();
    void annotationsChanged();

private:
    void emitSceneChanged();
    MarkupsScene *sceneForId(const QString &volumeId) const;
    void rebindActiveScene();

    MedicalDataController *m_medicalData = nullptr;
    std::map<QString, MarkupsScene> m_scenes;
    MarkupsScene m_scene;          // 当前活动数据集的 scene（默认空）
    QString m_activeVolumeId;
    int m_lastRevision = -1;
};
