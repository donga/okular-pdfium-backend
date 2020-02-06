/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QPDFIUM_PAGE_H
#define QPDFIUM_PAGE_H

#include <pdfium/fpdf_doc.h>

#include <QSharedPointer>
#include <QString>
#include <QList>

#include <okular/core/document.h>

namespace Okular {
class ObjectRect;
class Action;
class DocumentViewport;
}

namespace QPdfium {

struct CharEntity
{
    QString str;
    QRect area;
};

class PagePrivate;
class Page
{
public:
    Page(FPDF_DOCUMENT pdfdoc, int pageNumber, const QSizeF &dpi);
    ~Page();

    FPDF_PAGE getPdfPage();
    FPDF_TEXTPAGE getPdfTextPage();
    void closePdfPage();
    void closePdfTextPage();

    int pageNumber() const;
    QSizeF size() const;
    QString label() const;
    Okular::Rotation orientation() const;
    int numChars() const;
    int numRects() const;
    QList<CharEntity*> charEntityList() const;
    bool hasLinks();
    QLinkedList<Okular::ObjectRect*> links() const;
    QImage image(const int &width, const int &height);
    QImage renderToImage(float dpiX, float dpiY, int x, int y, int width, int height, Okular::Rotation rotation);

private:
    QSharedPointer<PagePrivate> d;
};

}

#endif //QPDFIUM_PAGE_H
