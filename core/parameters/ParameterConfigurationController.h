#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QSortFilterProxyModel>

class Mavlink;
class ParameterManager;

// File/comparison presentation only. ParameterManager owns all parameter transactions.
class ParameterConfigurationController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* rows READ rows CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(bool comparisonCurrent READ comparisonCurrent NOTIFY changed)
    Q_PROPERTY(bool canCompare READ canCompare NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString compatibility READ compatibility NOTIFY changed)
    Q_PROPERTY(QString firmware READ firmware NOTIFY changed)
    Q_PROPERTY(QVariantMap importSummary READ importSummary NOTIFY changed)
    Q_PROPERTY(QVariantMap progress READ progress NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
public:
    enum Roles { NameRole = Qt::UserRole + 1, CurrentRole, FileRole, TypeRole, StatusRole,
                 DetailRole, EligibleRole, SelectedRole, OutcomeRole, AttemptsRole, IdRole, SourceRowRole };
    struct Document { QVariantList entries; QString family = "unknown", format, error; int duplicates = 0; };
    explicit ParameterConfigurationController(Mavlink *transport, ParameterManager *manager);
    QAbstractItemModel *rows() { return &proxy; }
    QString filter() const { return rowFilter; }
    void setFilter(const QString &value);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool busy() const { return working; }
    bool applying() const { return operation; }
    bool comparisonCurrent() const { return compared; }
    bool canCompare() const;
    int selectedCount() const;
    QString message() const { return statusMessage; }
    QString compatibility() const;
    QString firmware() const { return snapshotFamily; }
    QVariantMap importSummary() const;
    QVariantMap progress() const;
    Q_INVOKABLE void importFile(const QUrl &url);
    Q_INVOKABLE void exportFile(const QUrl &url, bool acknowledgeStale, bool acknowledgeUnknown);
    Q_INVOKABLE void compare();
    Q_INVOKABLE void selectRow(int row, bool selected);
    Q_INVOKABLE void selectAll(bool selected);
    Q_INVOKABLE void applySelected(const QStringList &draftNames, bool acknowledgeUnknown, bool replaceDraftConflicts);
    Q_INVOKABLE void stop();
    static Document parse(const QByteArray &bytes, const QString &extension);
    static QVariantList compareEntries(const Document &document, const QVariantMap &snapshot, int encoding = 0, const QString &family = "unknown");
    static QString valueError(double value, int type);
    static QString typeLabel(int type);
    static QByteArray serialize(const QVariantMap &snapshot, const QVariantMap &provenance, const QString &family, bool stale);
signals:
    void changed();
private:
    void replaceRows(QVariantList rows);
    void setRow(int index, const QVariantMap &row);
    void invalidate();
    void submitNext();
    void finishedWrite(const QVariantMap &result);
    bool currentSource() const;
    QString liveFamily() const;
    void finishOperation();
    void markRemaining(const QString &reason);
    Mavlink *transport;
    QPointer<ParameterManager> manager;
    QSortFilterProxyModel proxy;
    QString rowFilter = "All";
    Document document;
    QVariantList comparisonRows;
    QFutureWatcher<Document> importer;
    QFutureWatcher<QVariantList> comparer;
    QFutureWatcher<QString> exporter;
    QTimer submitTimer;
    bool working = false, operation = false, compared = false, stopping = false;
    quint64 comparisonRevision = 0, sourceGeneration = 0, sourceConnection = 0;
    quint64 pendingId = 0;
    int pendingRow = -1, cursor = 0, submitted = 0;
    int selectedTotal = 0;
    QHash<QString,int> outcomeCounts;
    QList<int> applicationRows;
    QString snapshotFamily = "unknown", comparedFamily, statusMessage;
};
