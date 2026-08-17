#include "AiCommitService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

namespace Cherry {

AiCommitService::AiCommitService(QObject *parent)
    : QObject(parent)
{
}

AiCommitService::~AiCommitService()
{
    cancel();
}

QString AiCommitService::buildSystemPrompt(const QString &style, bool includeDescription)
{
    QString prompt = "# Task\n"
                     "Generate a concise, high-quality Git commit message based on the provided file diffs and repository context.\n\n"
                     "# Output Format\n";

    if (includeDescription) {
        prompt += "<commit_title>\n"
                  "\n"
                  "<commit_description>\n\n"
                  "# Constraints\n";
    } else {
        prompt += "<commit_title>\n\n"
                  "# Constraints\n";
    }

    if (style == "repo_history") {
        prompt += "- Style: Strictly emulate the exact naming, formatting conventions, casing, and style of the recent repository commits provided in the context.\n";
    } else if (style == "conventional") {
        prompt += "- Style: Conventional Commits format with lowercase type and optional scope (e.g. feat(ui): ..., fix: ..., chore: ..., refactor: ..., docs: ..., test: ..., style: ..., build: ..., ci: ...).\n";
    } else if (style == "summary") {
        prompt += "- Style: Imperative mood summary sentence (e.g. \"Add support for...\", \"Fix crash when...\", \"Update dependency to...\").\n";
    } else if (style == "concise") {
        prompt += "- Style: Short, direct plain language summary of the changes.\n";
    } else if (style == "gitmoji") {
        prompt += "- Style: Gitmoji format starting with an appropriate gitmoji emoji followed by the commit summary (e.g. \":sparkles: add feature\", \":bug: fix issue\").\n";
    } else {
        prompt += "- Style: " + style + "\n";
    }

    if (includeDescription) {
        prompt += "- Title Length: The first line (commit title) MUST strictly be under 40 characters for every commit style.\n"
                  "- Blank Line: Leave exactly one blank line between the title and description body.\n"
                  "- Body: 2 to 4 concise bullet points explaining what was changed and why. Do not echo unchanged code.\n";
    } else {
        prompt += "- Title Only: Output strictly one single line. Do not include any blank lines, description body, or bullet points.\n"
                  "- Title Length: Under 50 characters.\n";
    }

    prompt += "- Determinism: Output ONLY the raw commit text. Do not output markdown code blocks (no ```), greetings, or meta commentary.\n";

    return prompt;
}

QString AiCommitService::buildUserPrompt(bool followRepoStyle,
                                         const QList<CommitItem> &recentCommits,
                                         const QList<QPair<QString, QString>> &selectedFileDiffs)
{
    QString prompt;

    if (followRepoStyle && !recentCommits.isEmpty()) {
        prompt += "# Recent Repository Commits (Style Reference)\n";
        int count = qMin(5, static_cast<int>(recentCommits.size()));
        for (int i = 0; i < count; ++i) {
            const auto &c = recentCommits.at(i);
            prompt += QString("- Title: %1\n").arg(c.summary);
            if (!c.description.trimmed().isEmpty()) {
                prompt += QString("  Body: %1\n").arg(c.description.trimmed());
            }
        }
        prompt += "\n";
    }

    prompt += "# Staged / Selected File Changes\n";
    for (const auto &pair : selectedFileDiffs) {
        prompt += QString("=== File: %1 ===\n%2\n\n").arg(pair.first, pair.second);
    }

    return prompt;
}

void AiCommitService::cancel()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit generationFinished();
    }
}

void AiCommitService::generateCommitMessage(const QString &provider,
                                            const QString &apiKey,
                                            const QString &endpoint,
                                            const QString &model,
                                            const QString &commitStyle,
                                            bool includeDescription,
                                            bool followRepoStyle,
                                            const QList<CommitItem> &recentCommits,
                                            const QList<QPair<QString, QString>> &selectedFileDiffs)
{
    cancel();

    m_activeProvider = provider.toLower().trimmed();
    m_includeDescription = includeDescription;

    QString systemPrompt = buildSystemPrompt(commitStyle, includeDescription);
    QString userPrompt = buildUserPrompt(followRepoStyle, recentCommits, selectedFileDiffs);

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray requestBody;

    if (m_activeProvider == "openai" || m_activeProvider == "custom") {
        QString urlStr;
        if (m_activeProvider == "custom" && !endpoint.trimmed().isEmpty()) {
            urlStr = endpoint.trimmed();
            if (!urlStr.contains("/chat/completions")) {
                if (urlStr.endsWith("/")) {
                    urlStr += "chat/completions";
                } else {
                    urlStr += "/chat/completions";
                }
            }
        } else {
            urlStr = "https://api.openai.com/v1/chat/completions";
        }

        request.setUrl(QUrl(urlStr));
        if (!apiKey.trimmed().isEmpty()) {
            request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey.trimmed()).toUtf8());
        }

        QString chosenModel = model.trimmed();
        if (chosenModel.isEmpty()) {
            chosenModel = (m_activeProvider == "openai") ? "gpt-5.6-luna" : "deepseek-v4-flash";
        }

        QJsonObject rootObj;
        rootObj["model"] = chosenModel;
        rootObj["temperature"] = 0.2;

        QJsonArray messages;
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
        messages.append(sysMsg);

        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = userPrompt;
        messages.append(userMsg);

        rootObj["messages"] = messages;
        requestBody = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);

    } else if (m_activeProvider == "anthropic") {
        QString urlStr = endpoint.trimmed().isEmpty() ? "https://api.anthropic.com/v1/messages" : endpoint.trimmed();
        request.setUrl(QUrl(urlStr));
        if (!apiKey.trimmed().isEmpty()) {
            request.setRawHeader("x-api-key", apiKey.trimmed().toUtf8());
        }
        request.setRawHeader("anthropic-version", "2023-06-01");

        QString chosenModel = model.trimmed();
        if (chosenModel.isEmpty()) {
            chosenModel = "claude-sonnet-5";
        }

        QJsonObject rootObj;
        rootObj["model"] = chosenModel;
        rootObj["system"] = systemPrompt;
        rootObj["max_tokens"] = 1024;
        rootObj["temperature"] = 0.2;

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = userPrompt;
        messages.append(userMsg);

        rootObj["messages"] = messages;
        requestBody = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);

    } else if (m_activeProvider == "gemini") {
        QString chosenModel = model.trimmed();
        if (chosenModel.isEmpty()) {
            chosenModel = "gemini-3.5-flash-lite";
        }

        QString urlStr;
        if (!endpoint.trimmed().isEmpty()) {
            urlStr = endpoint.trimmed();
        } else {
            urlStr = QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(chosenModel);
        }

        request.setUrl(QUrl(urlStr));
        if (!apiKey.trimmed().isEmpty()) {
            request.setRawHeader("x-goog-api-key", apiKey.trimmed().toUtf8());
        }

        QJsonObject rootObj;

        QJsonObject sysInstruction;
        QJsonArray sysParts;
        QJsonObject sysPart;
        sysPart["text"] = systemPrompt;
        sysParts.append(sysPart);
        sysInstruction["parts"] = sysParts;
        rootObj["system_instruction"] = sysInstruction;

        QJsonArray contents;
        QJsonObject contentObj;
        contentObj["role"] = "user";
        QJsonArray userParts;
        QJsonObject userPart;
        userPart["text"] = userPrompt;
        userParts.append(userPart);
        contentObj["parts"] = userParts;
        contents.append(contentObj);
        rootObj["contents"] = contents;

        QJsonObject genConfig;
        genConfig["temperature"] = 0.2;
        rootObj["generationConfig"] = genConfig;

        requestBody = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
    } else {
        emit generationFailed(QString("Unknown AI provider: %1").arg(provider));
        return;
    }

    emit generationStarted();
    m_currentReply = m_network.post(request, requestBody);
    connect(m_currentReply, &QNetworkReply::finished, this, &AiCommitService::handleReplyFinished);
}

void AiCommitService::handleReplyFinished()
{
    if (!m_currentReply) return;

    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QString errStr = reply->errorString();
        QByteArray errData = reply->readAll();
        if (!errData.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(errData);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error")) {
                    if (obj["error"].isObject() && obj["error"].toObject().contains("message")) {
                        errStr = obj["error"].toObject()["message"].toString();
                    } else if (obj["error"].isString()) {
                        errStr = obj["error"].toString();
                    }
                }
            }
        }
        reply->deleteLater();
        emit generationFinished();
        emit generationFailed(errStr);
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (m_activeProvider == "openai" || m_activeProvider == "custom") {
        parseOpenAiResponse(data, m_includeDescription);
    } else if (m_activeProvider == "anthropic") {
        parseAnthropicResponse(data, m_includeDescription);
    } else if (m_activeProvider == "gemini") {
        parseGeminiResponse(data, m_includeDescription);
    }

    emit generationFinished();
}

void AiCommitService::parseOpenAiResponse(const QByteArray &data, bool includeDescription)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit generationFailed(tr("Invalid JSON response from AI service"));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        emit generationFailed(tr("No completions returned from AI service"));
        return;
    }

    QJsonObject choice = choices.first().toObject();
    QJsonObject message = choice["message"].toObject();
    QString content = message["content"].toString();

    processGeneratedText(content, includeDescription);
}

void AiCommitService::parseAnthropicResponse(const QByteArray &data, bool includeDescription)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit generationFailed(tr("Invalid JSON response from Anthropic API"));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray contentArray = root["content"].toArray();
    QString fullText;
    for (const auto &val : contentArray) {
        QJsonObject block = val.toObject();
        if (block["type"].toString() == "text") {
            fullText += block["text"].toString();
        }
    }

    if (fullText.isEmpty()) {
        emit generationFailed(tr("No text returned from Anthropic API"));
        return;
    }

    processGeneratedText(fullText, includeDescription);
}

void AiCommitService::parseGeminiResponse(const QByteArray &data, bool includeDescription)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit generationFailed(tr("Invalid JSON response from Gemini API"));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) {
        emit generationFailed(tr("No candidates returned from Gemini API"));
        return;
    }

    QJsonObject candidate = candidates.first().toObject();
    QJsonObject content = candidate["content"].toObject();
    QJsonArray parts = content["parts"].toArray();
    QString fullText;
    for (const auto &val : parts) {
        fullText += val.toObject()["text"].toString();
    }

    if (fullText.isEmpty()) {
        emit generationFailed(tr("No text returned from Gemini API"));
        return;
    }

    processGeneratedText(fullText, includeDescription);
}

void AiCommitService::processGeneratedText(const QString &rawText, bool includeDescription)
{
    QString clean = rawText.trimmed();

    // Strip wrapping markdown code blocks if the model included them
    if (clean.startsWith("```")) {
        int firstNl = clean.indexOf('\n');
        if (firstNl != -1) {
            clean = clean.mid(firstNl + 1);
        }
        if (clean.endsWith("```")) {
            clean.chop(3);
        }
        clean = clean.trimmed();
    }

    QString summary;
    QString description;

    if (!includeDescription) {
        int firstNl = clean.indexOf('\n');
        summary = (firstNl != -1) ? clean.left(firstNl).trimmed() : clean;
        description = "";
    } else {
        int firstNl = clean.indexOf('\n');
        if (firstNl != -1) {
            summary = clean.left(firstNl).trimmed();
            description = clean.mid(firstNl + 1).trimmed();
        } else {
            summary = clean;
            description = "";
        }
    }

    emit commitMessageGenerated(summary, description);
}

} // namespace Cherry
