#pragma once

#include <QString>

namespace Cherry {

class AvatarResolver {
public:
    // Resolves an avatar URL given author name, author email, and optionally repository remote URL.
    static QString resolve(const QString &authorName, const QString &authorEmail, const QString &repoRemoteUrl = QString());

    // Generates a Gravatar URL from an email string.
    static QString gravatarUrl(const QString &email, int size = 80);

    // Generates a Libravatar URL from an email string.
    static QString libravatarUrl(const QString &email, int size = 80);
};

} // namespace Cherry
