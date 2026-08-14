#include "AvatarResolver.h"
#include "AppSettings.h"
#include <QCryptographicHash>
#include <QRegularExpression>

namespace Cherry {

QString AvatarResolver::gravatarUrl(const QString &email, int size)
{
    const QString clean = email.trimmed().toLower();
    if (clean.isEmpty() || !clean.contains('@')) return QString();

    const QByteArray hash = QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("https://www.gravatar.com/avatar/%1?s=%2&d=404").arg(QString::fromLatin1(hash)).arg(size);
}

QString AvatarResolver::libravatarUrl(const QString &email, int size)
{
    const QString clean = email.trimmed().toLower();
    if (clean.isEmpty() || !clean.contains('@')) return QString();

    const QByteArray hash = QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("https://seccdn.libravatar.org/avatar/%1?s=%2&d=404").arg(QString::fromLatin1(hash)).arg(size);
}

QString AvatarResolver::resolve(const QString &authorName, const QString &authorEmail, const QString &repoRemoteUrl)
{
    Q_UNUSED(authorName);
    Q_UNUSED(repoRemoteUrl);

    const QString cleanEmail = authorEmail.trimmed();
    if (cleanEmail.isEmpty()) return QString();

    AppSettings *settings = AppSettings::instance();
    const QString provider = settings ? settings->avatarProvider() : QStringLiteral("auto");

    if (provider == QLatin1String("none")) {
        return QString();
    }
    if (provider == QLatin1String("libravatar")) {
        return libravatarUrl(cleanEmail);
    }
    if (provider == QLatin1String("gravatar")) {
        return gravatarUrl(cleanEmail);
    }

    // Auto resolution: Detect specific provider patterns
    // 1. GitHub noreply email with user ID: 12345+username@users.noreply.github.com
    static const QRegularExpression ghIdRegex(QStringLiteral(R"(^(\d+)\+([^@]+)@users\.noreply\.github\.com$)"), QRegularExpression::CaseInsensitiveOption);
    auto match = ghIdRegex.match(cleanEmail);
    if (match.hasMatch()) {
        const QString userId = match.captured(1);
        return QStringLiteral("https://avatars.githubusercontent.com/u/%1?s=80").arg(userId);
    }

    // 2. GitHub noreply email without ID: username@users.noreply.github.com
    static const QRegularExpression ghUserRegex(QStringLiteral(R"(^([^@]+)@users\.noreply\.github\.com$)"), QRegularExpression::CaseInsensitiveOption);
    match = ghUserRegex.match(cleanEmail);
    if (match.hasMatch()) {
        const QString username = match.captured(1);
        return QStringLiteral("https://github.com/%1.png?size=80").arg(username);
    }

    // 3. GitLab noreply email: username@noreply.gitlab.com / username@users.noreply.gitlab.com
    static const QRegularExpression glRegex(QStringLiteral(R"(^([^@]+)@(?:users\.)?noreply\.gitlab\.com$)"), QRegularExpression::CaseInsensitiveOption);
    match = glRegex.match(cleanEmail);
    if (match.hasMatch()) {
        const QString username = match.captured(1);
        return QStringLiteral("https://gitlab.com/%1.png?size=80").arg(username);
    }

    // 4. Codeberg noreply email: username@noreply.codeberg.org
    static const QRegularExpression cbRegex(QStringLiteral(R"(^([^@]+)@noreply\.codeberg\.org$)"), QRegularExpression::CaseInsensitiveOption);
    match = cbRegex.match(cleanEmail);
    if (match.hasMatch()) {
        const QString username = match.captured(1);
        return QStringLiteral("https://codeberg.org/%1.png").arg(username);
    }

    // 5. Standard email fallback via Gravatar with 404 fallback
    return gravatarUrl(cleanEmail);
}

} // namespace Cherry
