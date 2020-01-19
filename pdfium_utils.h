/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef PDFIUM_UTILS_H
#define PDFIUM_UTILS_H

#include <pdfium/fpdf_doc.h>
#include <pdfium/fpdfview.h>

#include <QString>
#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QFile>
#include <QDateTime>

namespace QPdfium {

    enum PageMode {
        PageMode_Unknown = -1,
        PageMode_UseNone = 0,
        PageMode_UseOutlines,
        PageMode_UseThumbs,
        PageMode_FullScreen,
        PageMode_UseOC,
        PageMode_UseAttachments
    };

    enum ActionType {
        Action_Unsopprted = 0,
        Action_Goto,
        Action_RemoteGoto,
        Action_URI,
        Action_Launch
    };

    QDateTime pdfiumDateToQDateTime(const QString &textDate);
    bool isWhiteSpace(const QString &str);
    QString GetPageLabel(FPDF_DOCUMENT pdfdoc, int pageNumber);
    QSizeF GetPageSizeF(FPDF_DOCUMENT pdfdoc, int pageNumber);
    QPointF GetLocationInPage(FPDF_DEST destination);
    QRectF FloatPageRectToPixelRect(FPDF_PAGE page, const QRectF &input);
    QRectF GetFloatCharRectInPixels(FPDF_PAGE page, FPDF_TEXTPAGE textPage, int index);

}

#endif //PDFIUM_UTILS_H
