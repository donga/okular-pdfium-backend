/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <pdfium/fpdfview.h>
#include <pdfium/fpdf_dataavail.h>
#include <pdfium/fpdf_doc.h>
#include <pdfium/fpdf_edit.h>
#include <pdfium/fpdf_ext.h>
#include <pdfium/fpdf_text.h>
#include <pdfium/fpdf_sysfontinfo.h>

#include <QFile>

#include <okular/core/document.h>
#include <okular/core/page.h>

#include "pdfium_utils.h"
#include "document.h"
#include "page.h"

namespace QPdfium {

class DocumentPrivate
{
public:
    bool loadDocument(const QString &filePath, const QByteArray &password)
    {
        this->filePath = filePath;
        if (!pdfdoc && (pdfdoc = FPDF_LoadDocument(QFile::encodeName(filePath).constData(), password.constData()))) {
            unsigned long err = FPDF_GetLastError();
            locked = (err == FPDF_ERR_PASSWORD);
            pagesCount = FPDF_GetPageCount(pdfdoc);
            pageMode = static_cast<PageMode>(FPDFDoc_GetPageMode(pdfdoc));
        }
        return (pdfdoc != nullptr);
    }

    bool unloadDocument()
    {
        if (pdfdoc) {
            FPDF_CloseDocument(pdfdoc);
            pdfdoc = nullptr;
        }
        return true;
    }

    QString metaText(const QByteArray &key) const
    {
        const unsigned long textLength = FPDF_GetMetaText(pdfdoc, key.constData(), nullptr, 0);
        QVector<ushort> buffer(textLength);
        FPDF_GetMetaText(pdfdoc, key.constData(), buffer.data(), buffer.length());

        return QString::fromUtf16(buffer.data());
    }

public:
    QString filePath;
    FPDF_DOCUMENT pdfdoc {nullptr};
    int pagesCount {-1};
    Okular::DocumentSynopsis *synopsis {nullptr};
    bool locked {false};
    PageMode pageMode {PageMode_Unknown};
};

Document::Document(const QString &filePath, const QString &password)
  : d(new DocumentPrivate())
{
    d->loadDocument(filePath, password.toLatin1());
}

Document::~Document()
{
}

FPDF_DOCUMENT Document::pdfdoc() const
{
    return d->pdfdoc;
}

Document *Document::load(const QString &filePath, const QString &password)
{
    return new Document(filePath, password);
}

bool Document::isLocked() const
{
    return d->locked;
}

bool Document::unlock(const QByteArray &password)
{
    return d->loadDocument(d->filePath, password);
}

PagePtr Document::page(int pageNumber) const
{
    return PagePtr(new Page(d->pdfdoc, pageNumber));
}

int Document::pagesCount() const
{
    return d->pagesCount;
}

PageMode Document::pageMode() const
{
    return d->pageMode;
}

QString Document::metaText(const QByteArray &key) const
{
    return d->metaText(key);
}

}
