#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

#include <array>
#include <memory>
#include <mutex>
#include <vector>

struct VolumeSnapshot
{
    std::array<int, 3> dimensions {0, 0, 0};
    std::array<double, 3> spacing {1.0, 1.0, 1.0};
    std::array<double, 3> origin {0.0, 0.0, 0.0};
    std::array<double, 9> direction {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::vector<short> pixels;
};

struct MaskSnapshot
{
    std::array<int, 3> dimensions {0, 0, 0};
    std::array<double, 3> spacing {1.0, 1.0, 1.0};
    std::array<double, 3> origin {0.0, 0.0, 0.0};
    std::array<double, 9> direction {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::vector<unsigned char> pixels;
};

struct DicomSeriesCandidate;

class MedicalDataController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY dataChanged)
    Q_PROPERTY(bool volumeData READ volumeData NOTIFY dataChanged)
    Q_PROPERTY(bool segmentationAvailable READ segmentationAvailable NOTIFY segmentationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString patientName READ patientName NOTIFY dataChanged)
    Q_PROPERTY(QString patientId READ patientId NOTIFY dataChanged)
    Q_PROPERTY(QString patientSex READ patientSex NOTIFY dataChanged)
    Q_PROPERTY(QString patientBirthDate READ patientBirthDate NOTIFY dataChanged)
    Q_PROPERTY(QString modality READ modality NOTIFY dataChanged)
    Q_PROPERTY(QString studyDescription READ studyDescription NOTIFY dataChanged)
    Q_PROPERTY(QString studyDate READ studyDate NOTIFY dataChanged)
    Q_PROPERTY(QString seriesDescription READ seriesDescription NOTIFY dataChanged)
    Q_PROPERTY(QString dimensionsText READ dimensionsText NOTIFY dataChanged)
    Q_PROPERTY(QString spacingText READ spacingText NOTIFY dataChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY dataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowingChanged)
    Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowingChanged)
    Q_PROPERTY(int datasetRevision READ datasetRevision NOTIFY dataChanged)
    Q_PROPERTY(int segmentationRevision READ segmentationRevision NOTIFY segmentationChanged)
    Q_PROPERTY(bool regionGrowingSeedValid READ regionGrowingSeedValid NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedX READ regionGrowingSeedX NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedY READ regionGrowingSeedY NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedZ READ regionGrowingSeedZ NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(int regionGrowingSeedValue READ regionGrowingSeedValue NOTIFY regionGrowingSeedChanged)
    Q_PROPERTY(QVariantList seriesChoices READ seriesChoices NOTIFY seriesChoicesChanged)
    Q_PROPERTY(int selectedSeriesIndex READ selectedSeriesIndex NOTIFY selectedSeriesIndexChanged)

public:
    explicit MedicalDataController(QObject *parent = nullptr);

    bool loaded() const;
    bool volumeData() const;
    bool segmentationAvailable() const;
    bool busy() const { return m_busy; }
    QString patientName() const { return m_patientName; }
    QString patientId() const { return m_patientId; }
    QString patientSex() const { return m_patientSex; }
    QString patientBirthDate() const { return m_patientBirthDate; }
    QString modality() const { return m_modality; }
    QString studyDescription() const { return m_studyDescription; }
    QString studyDate() const { return m_studyDate; }
    QString seriesDescription() const { return m_seriesDescription; }
    QString dimensionsText() const;
    QString spacingText() const;
    QString sourcePath() const { return m_sourcePath; }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }
    double windowWidth() const { return m_windowWidth; }
    double windowLevel() const { return m_windowLevel; }
    int datasetRevision() const { return m_datasetRevision; }
    int segmentationRevision() const { return m_segmentationRevision; }
    bool regionGrowingSeedValid() const { return m_regionGrowingSeedValid; }
    int regionGrowingSeedX() const { return m_regionGrowingSeed[0]; }
    int regionGrowingSeedY() const { return m_regionGrowingSeed[1]; }
    int regionGrowingSeedZ() const { return m_regionGrowingSeed[2]; }
    int regionGrowingSeedValue() const { return m_regionGrowingSeedValue; }
    QVariantList seriesChoices() const { return m_seriesChoices; }
    int selectedSeriesIndex() const { return m_selectedSeriesIndex; }

    std::shared_ptr<const VolumeSnapshot> volumeSnapshot() const;
    std::shared_ptr<const MaskSnapshot> maskSnapshot() const;

    Q_INVOKABLE bool importDicom(const QUrl &source);
    Q_INVOKABLE void importDicomAsync(const QUrl &source);
    Q_INVOKABLE bool selectSeries(int index);
    Q_INVOKABLE bool exportDicomCopy(const QUrl &destination);
    Q_INVOKABLE void loadDemoVolume();
    Q_INVOKABLE bool applyThreshold(double lower, double upper);
    Q_INVOKABLE bool setRegionGrowingSeed(int seedX, int seedY, int seedZ);
    Q_INVOKABLE void clearRegionGrowingSeed();
    Q_INVOKABLE bool applyRegionGrowingFromSeed(double lower, double upper);
    Q_INVOKABLE bool applyRegionGrowing(int seedX, int seedY, int seedZ,
                                       double lower, double upper);
    Q_INVOKABLE void clearSegmentation();
    Q_INVOKABLE double estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                          double viewportWidth, double viewportHeight) const;

public slots:
    void setWindowWidth(double value);
    void setWindowLevel(double value);

signals:
    void dataChanged();
    void segmentationChanged();
    void regionGrowingSeedChanged();
    void windowingChanged();
    void statusChanged();
    void busyChanged();
    void seriesChoicesChanged();
    void selectedSeriesIndexChanged();

private:
    void setBusy(bool busy);
    void setError(const QString &message);
    void installVolume(std::shared_ptr<VolumeSnapshot> snapshot,
                       const QStringList &sourceFiles);
    void resetMetadata();
    void publishSeriesCandidates(
        std::vector<std::shared_ptr<DicomSeriesCandidate>> candidates);
    bool loadSeriesCandidate(int index);

    mutable std::mutex m_snapshotMutex;
    std::shared_ptr<VolumeSnapshot> m_volume;
    std::shared_ptr<MaskSnapshot> m_mask;
    QStringList m_sourceFiles;
    std::vector<std::shared_ptr<DicomSeriesCandidate>> m_seriesCandidates;
    QVariantList m_seriesChoices;
    int m_selectedSeriesIndex = -1;

    QString m_patientName;
    QString m_patientId;
    QString m_patientSex;
    QString m_patientBirthDate;
    QString m_modality;
    QString m_studyDescription;
    QString m_studyDate;
    QString m_seriesDescription;
    QString m_sourcePath;
    QString m_statusMessage;
    QString m_errorMessage;
    double m_windowWidth = 400.0;
    double m_windowLevel = 40.0;
    int m_datasetRevision = 0;
    int m_segmentationRevision = 0;
    std::array<int, 3> m_regionGrowingSeed {-1, -1, -1};
    int m_regionGrowingSeedValue = 0;
    bool m_regionGrowingSeedValid = false;
    bool m_busy = false;
};
