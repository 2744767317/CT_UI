#pragma once

#include "src/dicom/medicaldatacontroller.h"
#include "src/markups/markupsscene.h"

#include <QObject>
#include <QVariantList>

class AnnotationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int toolType READ toolType WRITE setToolType NOTIFY toolChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool hasActive READ hasActive NOTIFY annotationsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList activePoints READ activePoints NOTIFY annotationsChanged)
    Q_PROPERTY(QString activeLabelPreview READ activeLabelPreview NOTIFY annotationsChanged)

public:
    enum ToolType {
        NoneTool = 0,
        MarkTool = 1,
        LengthTool = 2,
        AngleTool = 3,
        PerimeterTool = 4
    };
    Q_ENUM(ToolType)

    explicit AnnotationController(QObject *parent = nullptr);

    int toolType() const;
    bool visible() const;
    bool hasActive() const;
    int revision() const;
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
    Q_INVOKABLE bool finishActive();
    Q_INVOKABLE void cancelActive();
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE bool updateControlPoint(int nodeId, int pointIndex,
                                        double x, double y, double z);
    Q_INVOKABLE bool updateControlPointFromVoxel(int nodeId, int pointIndex,
                                                 int voxelX, int voxelY, int voxelZ);

    /// 供 VTK Representation 读取（世界坐标点）。
    Q_INVOKABLE QVariantList renderItems() const;

public slots:
    void onMedicalDataChanged();

signals:
    void toolChanged();
    void visibleChanged();
    void annotationsChanged();

private:
    void emitSceneChanged();

    MedicalDataController *m_medicalData = nullptr;
    MarkupsScene m_scene;
    int m_lastRevision = -1;
};
