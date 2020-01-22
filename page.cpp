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

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QGuiApplication>

#include <okular/core/action.h>
#include <okular/core/document.h>
#include <okular/core/page.h>

#include "pdfium_utils.h"
#include "page.h"

namespace QPdfium {

class PagePrivate
{
public:
    PagePrivate(FPDF_DOCUMENT pdfdoc, int pageNumber, const QSizeF &dpi)
    {
        this->pdfdoc = pdfdoc;
        this->pageNumber = pageNumber;
        this->dpi = dpi;
    }

    ~PagePrivate()
    {
        closeTextPage();
        closePage();
        clearCharEntityList();
    }

    Okular::Rotation getOrientation()
    {
        if (getPage()) {
            const int fzOrientation = FPDFPage_GetRotation(fzPage);
            switch (fzOrientation)
            {
                case 1: orientation = Okular::Rotation90;  break;
                case 2: orientation = Okular::Rotation180; break;
                case 3: orientation = Okular::Rotation270; break;
                case 0: orientation = Okular::Rotation0;   break;
            }
        }
        return orientation;
    }

    FPDF_PAGE getPage()
    {
        if (!fzPage) {
            fzPage = FPDF_LoadPage(pdfdoc, pageNumber);
        }
        return fzPage;
    }

    void closePage()
    {
        if (fzPage) {
            FPDF_ClosePage(fzPage);
            fzPage = nullptr;
        }
    }

    FPDF_TEXTPAGE getTextPage()
    {
        if (!textPage && getPage()) {
            textPage = FPDFText_LoadPage(getPage());
            numChars = FPDFText_CountChars(textPage);
            numRects = FPDFText_CountRects(textPage, 0, numChars);
        }
        return textPage;
    }

    void closeTextPage()
    {
        if (textPage) {
            FPDFText_ClosePage(textPage);
            textPage = nullptr;
        }
    }

    QString getPageLabel()
    {
        if (pdfdoc && pageLabel.isNull()) {
            pageLabel = QPdfium::GetPageLabel(pdfdoc, pageNumber);
        }
        return pageLabel;
    }

    QSizeF getPageSize()
    {
        if (pageSize.isEmpty() && pdfdoc) {
            pageSize = QPdfium::GetPageSizeF(pdfdoc, pageNumber);
        }
        return pageSize;
    }

    QImage image(const int &width, const int &height)
    {
        if ((cachedImage.size() != QSize(width, height)) || (cachedImage.isNull() && getPage())) {
            QImage image(width, height, QImage::Format_RGBA8888);
            image.setDevicePixelRatio(qGuiApp->devicePixelRatio());
            if (FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(image.width(), image.height(), FPDFBitmap_BGRA, image.bits(), image.bytesPerLine())) {
                image.fill(0xFFFFFFFF);
                FPDF_RenderPageBitmap(bitmap, fzPage, 0, 0, image.width(), image.height(), 0, FPDF_ANNOT | FPDF_LCD_TEXT | FPDF_REVERSE_BYTE_ORDER);// | FPDF_PRINTING);
                FPDFBitmap_Destroy(bitmap);
            }
            cachedImage = image;
        }
        return cachedImage;
    }

    void clearCharEntityList()
    {
        while (!charEntityList.isEmpty())
            delete charEntityList.takeFirst();
    }

    QList<CharEntity*> getCharEntityList()
    {
        if (!getTextPage())
            return charEntityList;

        if (!charEntityList.isEmpty())
            return charEntityList;

        int rectIdx = 0;
        QRectF lineRect;
        for (int idx = 0; idx < numChars; ++idx) {
            CharEntity *entity = new CharEntity;
            charEntityList.append(entity);

            const ushort unicode = FPDFText_GetUnicode(textPage, idx);
            QString chStr;
            chStr += unicode;

            QRectF charBox = QPdfium::GetFloatCharRectInPixels(fzPage, textPage, idx).normalized();
            if (charBox.width() <= 0.00001 || charBox.height() <= 0.00001) {
                if (idx-1 > 0) {
                    QRect chBox = charBox.toRect();
                    QRect lastBox = charEntityList.at(idx-1)->area;
                    QRect escRect;
                    if (unicode=='\r' || unicode=='\n') {
                        escRect = QRect(lastBox.x()+lastBox.width()-1, lastBox.y(), 1, lastBox.height());
                    }
                    else {
                        escRect = QRect(lastBox.right()-1, lastBox.y(), chBox.width(), lastBox.height());
                    }
                    charEntityList.at(idx)->str   = chStr;
                    charEntityList.at(idx)->area  = escRect;
                }
                continue;
            }

            if ((lineRect.isEmpty() || !lineRect.intersects(charBox)) && rectIdx+1 <= numRects) {
                double rt_left, rt_top, rt_right, rt_bottom;
                FPDFText_GetRect(textPage, rectIdx++, &rt_left, &rt_top, &rt_right, &rt_bottom);
                lineRect = QPdfium::FloatPageRectToPixelRect(fzPage, QRectF(rt_left, rt_top, rt_right - rt_left, rt_bottom - rt_top));
            }

            float ntop    = qMin(lineRect.top(), charBox.top());
            float nbottom = qMax(lineRect.bottom(), charBox.bottom());

            charEntityList.at(idx)->str  = chStr;
            charEntityList.at(idx)->area = QRectF(charBox.left(), ntop, charBox.width(), nbottom - ntop).toRect();

            if (idx-1 >= 0 && charEntityList.at(idx-1)->area.top() == charEntityList.at(idx)->area.top()) {
                charEntityList.at(idx-1)->area.setRight(charEntityList.at(idx)->area.left());
            }
        }

        return charEntityList;
    }
    
    bool hasLinks()
    {
        if (getPage() && !isHasLinks) {
            int linkPos = 0;
            FPDF_LINK linkAnnot;
            isHasLinks = FPDFLink_Enumerate(fzPage, &linkPos, &linkAnnot);
        }
        return isHasLinks;
    }

    QLinkedList<Okular::ObjectRect*> getLinks()
    {
        if (!getPage())
            return links;

        if (isLinksGenerated)
            return links;

        const qreal width   = pageSize.width();
        const qreal height  = pageSize.height();

        int linkPos = 0;
        FPDF_LINK linkAnnot;
        while (FPDFLink_Enumerate(fzPage, &linkPos, &linkAnnot)) {
            int targetPage = -1;
            QString uriStr;
            FS_RECTF rect;
            FPDF_DEST destination;
            bool hasRect = false;

            if ((destination = FPDFLink_GetDest(pdfdoc, linkAnnot))) {
                targetPage = FPDFDest_GetDestPageIndex(pdfdoc, destination);
            }

            if (FPDF_ACTION action = FPDFLink_GetAction(linkAnnot)) {
                const unsigned long uriLength = FPDFAction_GetURIPath(pdfdoc, action, nullptr, 0);
                QVector<char> uriBuffer(uriLength);  //7-bit ASCII
                FPDFAction_GetURIPath(pdfdoc, action, uriBuffer.data(), uriBuffer.length());
                uriStr = QString::fromLocal8Bit(uriBuffer.data());
            }

            hasRect = FPDFLink_GetAnnotRect(linkAnnot, &rect);

            if (hasRect && (targetPage != -1 || !uriStr.isNull())) {
                int devX, devY;
                qreal nWidth  = (rect.right - rect.left);
                qreal nHeight = (rect.bottom - rect.top);
                FPDF_PageToDevice(fzPage, 0, 0, width, height, 0, rect.left, rect.top, &devX, &devY);
                QRectF boundary = QRectF(devX/width, (devY - nHeight)/height, nWidth/width, nHeight/height);

                Okular::Action *okularAction = nullptr;
                if (targetPage != -1) { // internal link
                    Okular::DocumentViewport viewport(targetPage);
                    QPointF targetPointF = QPdfium::GetLocationInPage(destination);
                    if (!targetPointF.isNull()) {
                        auto targetSizeF = QPdfium::GetPageSizeF(pdfdoc, targetPage);
                        viewport.rePos.pos = Okular::DocumentViewport::TopLeft;
                        viewport.rePos.normalizedX = targetPointF.x() / targetSizeF.width();
                        viewport.rePos.normalizedY = (targetSizeF.height() - targetPointF.y()) / targetSizeF.height();
                        viewport.rePos.enabled = true;
                    }
                    okularAction = new Okular::GotoAction(uriStr, viewport);
                }
                else if (!uriStr.isNull()) { // external link
                    okularAction = new Okular::BrowseAction(QUrl(uriStr));
                }

                if (okularAction) {
                    Okular::ObjectRect *rect = new Okular::ObjectRect(
                                boundary.left(), boundary.top(), boundary.right(), boundary.bottom(), 
                                false, 
                                Okular::ObjectRect::Action, 
                                okularAction);
                    links.push_back(rect);
                }
            }

        }
        
        isLinksGenerated = true;
        
        return links;
    }

public:
    FPDF_DOCUMENT pdfdoc {nullptr};
    FPDF_PAGE fzPage {nullptr};
    FPDF_TEXTPAGE textPage {nullptr};
    QSizeF dpi {0.0, 0.0};
    QSizeF pageSize {0.0, 0.0};
    int pageNumber {-1};
    QString pageLabel;
    Okular::Rotation orientation {Okular::Rotation0};
    int numChars {-1};
    int numRects {-1};
    QImage cachedImage;
    QList<CharEntity*> charEntityList;
    QLinkedList<Okular::ObjectRect*> links;
    bool isHasLinks {false};
    bool isLinksGenerated {false};
    QMutex mutex;
};

Page::Page(FPDF_DOCUMENT pdfdoc, int pageNumber, const QSizeF &dpi)
  : d(new PagePrivate(pdfdoc, pageNumber, dpi))
{
}

Page::~Page()
{
}

FPDF_PAGE Page::getPdfPage()
{
    return d->getPage();
}

FPDF_TEXTPAGE Page::getPdfTextPage()
{
    return d->getTextPage();
}

void Page::closePdfPage()
{
    d->closePage();
}

void Page::closePdfTextPage()
{
    d->closeTextPage();
}

QSizeF Page::size() const
{
    return d->getPageSize();
}

QString Page::label() const
{
    return d->getPageLabel();
}

int Page::pageNumber() const
{
    return d->pageNumber;
}

Okular::Rotation Page::orientation() const
{
    return d->getOrientation();
}

int Page::numChars() const
{
    d->getTextPage();
    return d->numChars;
}

int Page::numRects() const
{
    d->getTextPage();
    return d->numRects;
}

QImage Page::image(const int &width, const int &height)
{
    QMutexLocker locker(&d->mutex);
    return d->image(width, height);
}

QList<CharEntity*> Page::charEntityList() const
{
    QMutexLocker locker(&d->mutex);
    return d->getCharEntityList();
}

bool Page::hasLinks()
{
    return d->hasLinks();
}

QLinkedList<Okular::ObjectRect*> Page::links() const
{
    QMutexLocker locker(&d->mutex);
    return d->getLinks();
}

}
