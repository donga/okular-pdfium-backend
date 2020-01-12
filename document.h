/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QPDFIUM_DOCUMENT_H
#define QPDFIUM_DOCUMENT_H

#include <QScopedPointer>
#include <QSharedPointer>
#include <QDateTime>

#include <okular/core/document.h>

#include "page.h"

namespace QPdfium {

typedef QSharedPointer<Page> PagePtr;

class Page;
class DocumentPrivate;
class Document {
public:
    ~Document();

    FPDF_DOCUMENT pdfdoc() const;
    bool isLocked() const;
    bool unlock(const QByteArray &password);
    int pagesCount() const;
    PageMode pageMode() const;
    PagePtr page(int pageNumber) const;
    QString metaText(const QByteArray &key) const;
    static Document *load(const QString &filePath, const QString &password = QString());

private:
    Document(const QString &filePath, const QString &password = QString());

private:
    QScopedPointer<DocumentPrivate> d;
    friend class Page;
};

}

#endif // QPDFIUM_DOCUMENT_H
