/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <pdfium/fpdf_text.h>

#include <QRegExp>

#include "pdfium_utils.h"

namespace QPdfium {

QDateTime pdfiumDateToQDateTime(const QString &textDate)
{
    QString text(textDate);
    // ** Code from QtPdfium **
    // convert a "D:YYYYMMDDHHmmSSOHH'mm'" into "YYYY-MM-DDTHH:mm:ss+HH:mm"
    if (text.startsWith(QLatin1String("D:")))
        text = text.mid(2);
    text.insert(4, QLatin1Char('-'));
    text.insert(7, QLatin1Char('-'));
    text.insert(10, QLatin1Char('T'));
    text.insert(13, QLatin1Char(':'));
    text.insert(16, QLatin1Char(':'));
    text.replace(QLatin1Char('\''), QLatin1Char(':'));
    if (text.endsWith(QLatin1Char(':')))
        text.chop(1);

    return QDateTime::fromString(text, Qt::ISODate);
}

QString GetPageLabel(FPDF_DOCUMENT pdfdoc, int pageNumber)
{
    const unsigned long labelLength = FPDF_GetPageLabel(pdfdoc, pageNumber, nullptr, 0);
    QVector<ushort> buffer(labelLength);
    FPDF_GetPageLabel(pdfdoc, pageNumber, buffer.data(), buffer.length());
    return QString::fromUtf16(buffer.data());
}

QSizeF GetPageSizeF(FPDF_DOCUMENT pdfdoc, int pageNumber)
{
    double width, height;
    FPDF_GetPageSizeByIndex(pdfdoc, pageNumber, &width, &height);
    return QSizeF(width, height);
}

QPointF GetLocationInPage(FPDF_DEST destination)
{
    FPDF_BOOL hasX = 0;
    FPDF_BOOL hasY = 0;
    FPDF_BOOL hasZoom = 0;
    FS_FLOAT x = 0.f;
    FS_FLOAT y = 0.f;
    FS_FLOAT zoom = 0.f;
    FPDFDest_GetLocationInPage(destination, &hasX, &hasY, &hasZoom, &x, &y, &zoom);

    return (hasX && hasY) ? QPointF(x, y) : QPointF();
}

QRectF FloatPageRectToPixelRect(FPDF_PAGE page, const QRectF &input)
{
    int outputWidth  = FPDF_GetPageWidth(page);
    int outputHeight = FPDF_GetPageHeight(page);

    FPDF_BOOL ret;
    int min_x, min_y;
    int max_x, max_y;
    ret = FPDF_PageToDevice(page, 0, 0, outputWidth, outputHeight, 0, input.x(), input.y(), &min_x, &min_y);
    if (!ret)
        return QRectF();
    ret = FPDF_PageToDevice(page, 0, 0, outputWidth, outputHeight, 0, input.right(), input.bottom(), &max_x, &max_y);
    if (!ret)
        return QRectF();

    if (max_x < min_x)
        std::swap(min_x, max_x);
    if (max_y < min_y)
        std::swap(min_y, max_y);

    return QRectF(min_x, min_y, max_x - min_x, max_y - min_y);
}

QRectF GetFloatCharRectInPixels(FPDF_PAGE page, FPDF_TEXTPAGE textPage, int index)
{
    double left, right, bottom, top;
    double ls_left, ls_right, ls_bottom, ls_top;

    FPDFText_GetCharBox(textPage, index, &left, &right, &bottom, &top);
    FPDFText_GetLooseCharBox(textPage, index, &ls_left, &ls_right, &ls_bottom, &ls_top);

    if (right < left) std::swap(left, right);
    if (bottom < top) std::swap(top, bottom);
    if (ls_right < ls_left) std::swap(ls_left, ls_right);
    if (ls_bottom < ls_top) std::swap(ls_top, ls_bottom);

    QRectF lsRect(ls_left, ls_top, ls_right - ls_left, ls_bottom - ls_top);
    //float delta = qAbs(lsRect.left() - ls_left);
    //left = qMin(lsRect.left(), ls_left);
    //top  = qMin(lsRect.top(), ls_top);
    //left  -= (delta / 1.f);
    right += lsRect.width();
    return FloatPageRectToPixelRect(page, QRectF(left, top, right - left, bottom - top));
}

bool isWhiteSpace(const QString &str)
{
  return QRegExp(QStringLiteral("\\s*")).exactMatch(str);
}

}
