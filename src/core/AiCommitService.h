#pragma once

#include "Types.h"
#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Cherry {

class AiCommitService : public QObject {
    Q_OBJECT

public:
    explicit AiCommitService(QObject *parent = nullptr);
    ~AiCommitService() override;

    void generateCommitMessage(const QString &provider,
                               const QString &apiKey,
                               const QString &endpoint,
                               const QString &model,
                               const QString &commitStyle,
                               bool includeDescription,
                               bool followRepoStyle,
                               const QList<CommitItem> &recentCommits,
                               const QList<QPair<QString, QString>> &selectedFileDiffs);

    void cancel();
    bool isGenerating() const { return m_currentReply != nullptr; }

    static QString buildSystemPrompt(const QString &style, bool includeDescription);
    static QString buildUserPrompt(bool followRepoStyle,
                                   const QList<CommitItem> &recentCommits,
                                   const QList<QPair<QString, QString>> &selectedFileDiffs);

signals:
    void commitMessageGenerated(const QString &summary, const QString &description);
    void generationFailed(const QString &errorMessage);
    void generationStarted();
    void generationFinished();

private slots:
    void handleReplyFinished();

private:
    void parseOpenAiResponse(const QByteArray &data, bool includeDescription);
    void parseAnthropicResponse(const QByteArray &data, bool includeDescription);
    void parseGeminiResponse(const QByteArray &data, bool includeDescription);
    void processGeneratedText(const QString &rawText, bool includeDescription);

    QNetworkAccessManager m_network;
    QNetworkReply *m_currentReply{nullptr};
    bool m_includeDescription{true};
    QString m_activeProvider;
};

} // namespace Cherry
