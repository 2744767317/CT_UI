#pragma once

#include "segmentationlabels.h"

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <memory>
#include <mutex>
#include <vector>

// 跨 GUI/VTK 渲染线程传递的不可变体数据快照。方向矩阵采用 DICOM LPS 坐标系。
struct VolumeSnapshot
{
    std::array<int, 3> dimensions {0, 0, 0};
    std::array<double, 3> spacing {1.0, 1.0, 1.0};
    std::array<double, 3> origin {0.0, 0.0, 0.0};
    std::array<double, 9> direction {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::vector<short> pixels;
};

// 分割结果与源 Volume 保持相同几何信息；0 为背景，1–4 为独立标签。
struct MaskSnapshot
{
    std::array<int, 3> dimensions {0, 0, 0};
    std::array<double, 3> spacing {1.0, 1.0, 1.0};
    std::array<double, 3> origin {0.0, 0.0, 0.0};
    std::array<double, 9> direction {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::vector<unsigned char> pixels;
};

struct DicomSeriesCandidate;
struct LoadedVolumeNode;
struct SeriesLoadResult;
struct SegmentationResult;

// 医学数据与场景控制器：负责 DICOM 发现、Volume 节点、显示状态和 ITK 算法。
// QML 不直接持有大块像素内存，只通过属性和只读快照访问当前激活节点。
class MedicalDataController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY dataChanged)
    Q_PROPERTY(bool volumeData READ volumeData NOTIFY dataChanged)
    Q_PROPERTY(bool projectionData READ projectionData NOTIFY dataChanged)
    Q_PROPERTY(bool pairedProjectionAvailable READ pairedProjectionAvailable NOTIFY dataChanged)
    Q_PROPERTY(bool segmentationAvailable READ segmentationAvailable NOTIFY segmentationChanged)
    Q_PROPERTY(QString segmentationMethod READ segmentationMethod NOTIFY segmentationChanged)
    Q_PROPERTY(qint64 segmentationVoxelCount READ segmentationVoxelCount NOTIFY segmentationChanged)
    Q_PROPERTY(double segmentationVolumeMl READ segmentationVolumeMl NOTIFY segmentationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString patientName READ patientName NOTIFY dataChanged)
    Q_PROPERTY(QString patientId READ patientId NOTIFY dataChanged)
    Q_PROPERTY(QString patientSex READ patientSex NOTIFY dataChanged)
    Q_PROPERTY(QString patientBirthDate READ patientBirthDate NOTIFY dataChanged)
    Q_PROPERTY(QString modality READ modality NOTIFY dataChanged)
    Q_PROPERTY(QString studyDescription READ studyDescription NOTIFY dataChanged)
    Q_PROPERTY(QString studyDate READ studyDate NOTIFY dataChanged)
    Q_PROPERTY(QString seriesDescription READ seriesDescription NOTIFY dataChanged)
    Q_PROPERTY(QString projectionViewLabel READ projectionViewLabel NOTIFY dataChanged)
    Q_PROPERTY(QString projectionPairViewLabel READ projectionPairViewLabel NOTIFY dataChanged)
    Q_PROPERTY(QString patientOrientation READ patientOrientation NOTIFY dataChanged)
    Q_PROPERTY(QString projectionPairOrientation READ projectionPairOrientation NOTIFY dataChanged)
    Q_PROPERTY(QString imageType READ imageType NOTIFY dataChanged)
    Q_PROPERTY(QString sopClassName READ sopClassName NOTIFY dataChanged)
    Q_PROPERTY(QString projectionPairImageType READ projectionPairImageType NOTIFY dataChanged)
    Q_PROPERTY(QString projectionPairSopClassName READ projectionPairSopClassName NOTIFY dataChanged)
    Q_PROPERTY(QString dimensionsText READ dimensionsText NOTIFY dataChanged)
    Q_PROPERTY(QString spacingText READ spacingText NOTIFY dataChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY dataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowingChanged)
    Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowingChanged)
    Q_PROPERTY(double displayWindowLevel READ displayWindowLevel NOTIFY windowingChanged)
    Q_PROPERTY(bool projectionUnsigned READ projectionUnsigned NOTIFY dataChanged)
    Q_PROPERTY(bool projectionInverted READ projectionInverted NOTIFY dataChanged)
    Q_PROPERTY(bool projectionPairInverted READ projectionPairInverted NOTIFY dataChanged)
    Q_PROPERTY(int datasetRevision READ datasetRevision NOTIFY dataChanged)
    Q_PROPERTY(int segmentationRevision READ segmentationRevision NOTIFY segmentationChanged)
    Q_PROPERTY(bool regionGrowingSeedValid READ regionGrowingSeedValid NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedX READ regionGrowingSeedX NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedY READ regionGrowingSeedY NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedZ READ regionGrowingSeedZ NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedValue READ regionGrowingSeedValue NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(QVariantList seriesChoices READ seriesChoices NOTIFY seriesChoicesChanged)
    Q_PROPERTY(int selectedSeriesIndex READ selectedSeriesIndex NOTIFY selectedSeriesIndexChanged)
    Q_PROPERTY(QVariantList volumeNodes READ volumeNodes NOTIFY volumeNodesChanged)
    Q_PROPERTY(int selectedVolumeIndex READ selectedVolumeIndex NOTIFY volumeNodesChanged)
    Q_PROPERTY(bool activeVolumeVisible READ activeVolumeVisible NOTIFY volumeNodesChanged)
    Q_PROPERTY(QVariantList operationHistory READ operationHistory NOTIFY operationHistoryChanged)
    Q_PROPERTY(int currentSegmentationLabel READ currentSegmentationLabel WRITE setCurrentSegmentationLabel NOTIFY currentSegmentationLabelChanged)
    Q_PROPERTY(QColor currentSegmentationLabelColor READ currentSegmentationLabelColor NOTIFY currentSegmentationLabelChanged)
    Q_PROPERTY(QVariantList segmentationLabels READ segmentationLabels CONSTANT)

public:
    explicit MedicalDataController(QObject *parent = nullptr);

    bool loaded() const;
    bool volumeData() const;
    bool projectionData() const;
    bool pairedProjectionAvailable() const;
    bool segmentationAvailable() const;
    QString segmentationMethod() const { return m_segmentationMethod; }
    qint64 segmentationVoxelCount() const { return m_segmentationVoxelCount; }
    double segmentationVolumeMl() const { return m_segmentationVolumeMl; }
    bool busy() const { return m_busy; }
    QString patientName() const { return m_patientName; }
    QString patientId() const { return m_patientId; }
    QString patientSex() const { return m_patientSex; }
    QString patientBirthDate() const { return m_patientBirthDate; }
    QString modality() const { return m_modality; }
    QString studyDescription() const { return m_studyDescription; }
    QString studyDate() const { return m_studyDate; }
    QString seriesDescription() const { return m_seriesDescription; }
    QString projectionViewLabel() const { return m_projectionViewLabel; }
    QString projectionPairViewLabel() const { return m_projectionPairViewLabel; }
    QString patientOrientation() const { return m_patientOrientation; }
    QString projectionPairOrientation() const { return m_projectionPairOrientation; }
    QString imageType() const { return m_imageType; }
    QString sopClassName() const { return m_sopClassName; }
    QString projectionPairImageType() const { return m_projectionPairImageType; }
    QString projectionPairSopClassName() const { return m_projectionPairSopClassName; }
    QString dimensionsText() const;
    QString spacingText() const;
    QString sourcePath() const { return m_sourcePath; }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }
    double windowWidth() const { return m_windowWidth; }
    double windowLevel() const { return m_windowLevel; }
    double displayWindowLevel() const;
    bool projectionUnsigned() const { return m_projectionUnsigned; }
    bool projectionInverted() const { return m_projectionInverted; }
    bool projectionPairInverted() const { return m_projectionPairInverted; }
    int datasetRevision() const { return m_datasetRevision; }
    int segmentationRevision() const { return m_segmentationRevision; }
    bool regionGrowingSeedValid() const { return m_regionGrowingSeedValid; }
    int regionGrowingSeedX() const { return m_regionGrowingSeed[0]; }
    int regionGrowingSeedY() const { return m_regionGrowingSeed[1]; }
    int regionGrowingSeedZ() const { return m_regionGrowingSeed[2]; }
    int regionGrowingSeedValue() const { return m_regionGrowingSeedValue; }
    QVariantList seriesChoices() const { return m_seriesChoices; }
    int selectedSeriesIndex() const { return m_selectedSeriesIndex; }
    QVariantList volumeNodes() const;
    int selectedVolumeIndex() const { return m_selectedVolumeIndex; }
    bool activeVolumeVisible() const;
    QVariantList operationHistory() const { return m_operationHistory; }
    int currentSegmentationLabel() const { return m_currentSegmentationLabel; }
    QColor currentSegmentationLabelColor() const
    {
        const auto &entry = SegmentationLabels::entry(m_currentSegmentationLabel);
        return QColor::fromRgbF(entry.r, entry.g, entry.b);
    }
    QVariantList segmentationLabels() const
    {
        QVariantList list;
        for (const auto &entry : SegmentationLabels::kEntries) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), static_cast<int>(entry.id));
            item.insert(QStringLiteral("name"), QString::fromUtf8(entry.name));
            item.insert(QStringLiteral("color"), QColor::fromRgbF(entry.r, entry.g, entry.b));
            list.push_back(item);
        }
        return list;
    }

    std::shared_ptr<const VolumeSnapshot> volumeSnapshot() const;
    std::shared_ptr<const VolumeSnapshot> projectionPairSnapshot() const;
    std::shared_ptr<const MaskSnapshot> maskSnapshot() const;

    Q_INVOKABLE bool importDicom(const QUrl &source);
    Q_INVOKABLE void importDicomAsync(const QUrl &source);
    Q_INVOKABLE bool selectSeries(int index);
    Q_INVOKABLE void selectSeriesAsync(int index);
    Q_INVOKABLE bool selectVolume(int index);
    Q_INVOKABLE bool renameVolume(int index, const QString &name);
    /// 当前活动数据集的稳定 id（供标注按数据集绑定）。
    Q_INVOKABLE QString activeVolumeId() const;
    Q_INVOKABLE bool removeVolume(int index);
    Q_INVOKABLE bool setVolumeVisibility(int index, bool visible);
    Q_INVOKABLE bool exportDicomCopy(const QUrl &destination);
    Q_INVOKABLE bool exportCasePackage(const QUrl &destination,
                                       const QVariantList &annotations);
    Q_INVOKABLE bool exportSecondaryCapture(const QUrl &destination,
                                            const QByteArray &rgbPackedTopLeft,
                                            int width, int height,
                                            bool includeAnnotations = false);
    Q_INVOKABLE bool exportVolumeRenderCapture(const QUrl &destination,
                                               const QByteArray &rgbPackedTopLeft,
                                               int width, int height,
                                               bool includeAnnotations = false);
    Q_INVOKABLE void reportError(const QString &message) { setError(message); }
    Q_INVOKABLE void reportStatus(const QString &message)
    {
        m_statusMessage = message;
        m_errorMessage.clear();
        emit statusChanged();
    }
    Q_INVOKABLE void loadDemoVolume();
    Q_INVOKABLE bool applyThreshold(double lower, double upper);
    Q_INVOKABLE void applyThresholdAsync(double lower, double upper);
    Q_INVOKABLE bool setRegionGrowingSeed(int seedX, int seedY, int seedZ);
    Q_INVOKABLE void clearRegionGrowingSeed();
    Q_INVOKABLE bool applyRegionGrowingFromSeed(double lower, double upper);
    Q_INVOKABLE void applyRegionGrowingFromSeedAsync(double lower, double upper,
                                                     bool fullyConnected);
    Q_INVOKABLE bool applyRegionGrowing(int seedX, int seedY, int seedZ,
                                       double lower, double upper,
                                       bool fullyConnected = false);
    Q_INVOKABLE void clearSegmentation();
    Q_INVOKABLE double estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                          double viewportWidth, double viewportHeight,
                                          bool pairedProjection = false) const;
    // Update width and level together so interactive drags publish one render update.
    Q_INVOKABLE void setWindowing(double width, double level);

public slots:
    void setWindowWidth(double value);
    void setWindowLevel(double value);
    void setCurrentSegmentationLabel(int label)
    {
        const int clamped = SegmentationLabels::clamp(label);
        if (m_currentSegmentationLabel == clamped)
            return;
        m_currentSegmentationLabel = clamped;
        emit currentSegmentationLabelChanged();
    }

signals:
    void dataChanged();
    void segmentationChanged();
    void regionGrowingSeedChanged();
    void windowingChanged();
    void statusChanged();
    void busyChanged();
    void seriesChoicesChanged();
    void selectedSeriesIndexChanged();
    void volumeNodesChanged();
    void operationHistoryChanged();
    void currentSegmentationLabelChanged();
    void casePackageAnnotationsReady(const QVariantList &items);

private:
    void setBusy(bool busy);
    void setError(const QString &message);
    void installVolume(std::shared_ptr<VolumeSnapshot> snapshot,
                       const QStringList &sourceFiles,
                       std::shared_ptr<VolumeSnapshot> pairSnapshot = {},
                       const QStringList &pairSourceFiles = {});
    void resetMetadata();
    void publishSeriesCandidates(
        std::vector<std::shared_ptr<DicomSeriesCandidate>> candidates);
    bool loadSeriesCandidate(int index);
    bool commitSeriesLoad(SeriesLoadResult result);
    bool commitSegmentation(SegmentationResult result, int expectedDatasetRevision,
                            const QString &successMessage);
    void activateVolumeNode(int index);
    void updateActiveVolumeNode();
    void clearActiveVolume();
    void recordOperation(const QString &type, const QVariantMap &parameters = {});
    bool prepareCasePackage(const QString &path, QString *scanPath);

    mutable std::mutex m_snapshotMutex;
    std::shared_ptr<VolumeSnapshot> m_volume;
    std::shared_ptr<VolumeSnapshot> m_projectionPair;
    std::shared_ptr<MaskSnapshot> m_mask;
    QStringList m_sourceFiles;
    std::vector<std::shared_ptr<DicomSeriesCandidate>> m_seriesCandidates;
    QVariantList m_seriesChoices;
    int m_selectedSeriesIndex = -1;
    // 工作区可同时驻留多个 Volume；渲染层每次消费 selectedVolume 对应的快照。
    std::vector<std::shared_ptr<LoadedVolumeNode>> m_volumeNodes;
    int m_selectedVolumeIndex = -1;

    QString m_patientName;
    QString m_patientId;
    QString m_patientSex;
    QString m_patientBirthDate;
    QString m_modality;
    QString m_studyDescription;
    QString m_studyDate;
    QString m_studyInstanceUid;
    QString m_seriesDescription;
    QString m_projectionViewLabel;
    QString m_projectionPairViewLabel;
    QString m_patientOrientation;
    QString m_projectionPairOrientation;
    QString m_imageType;
    QString m_sopClassName;
    QString m_projectionPairImageType;
    QString m_projectionPairSopClassName;
    bool m_projectionUnsigned = false;
    bool m_projectionInverted = false;
    bool m_projectionPairInverted = false;
    QString m_sourcePath;
    QString m_statusMessage;
    QString m_errorMessage;
    double m_windowWidth = 400.0;
    double m_windowLevel = 40.0;
    int m_datasetRevision = 0;
    int m_segmentationRevision = 0;
    QString m_segmentationMethod;
    qint64 m_segmentationVoxelCount = 0;
    double m_segmentationVolumeMl = 0.0;
    std::array<int, 3> m_regionGrowingSeed {-1, -1, -1};
    int m_regionGrowingSeedValue = 0;
    bool m_regionGrowingSeedValid = false;
    bool m_busy = false;
    QVariantList m_operationHistory;
    QString m_pendingCasePackageRoot;
    QVariantMap m_pendingCasePackage;
    int m_currentSegmentationLabel = SegmentationLabels::SoftTissue;
};
