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

#include <QSharedData>
#include <QWeakPointer>
#include <QScopedPointer>
#include <QLocale>
#include <QDateTime>
#include <QMatrix>
#include <QImage>
#include <QMutexLocker>
#include <QCoreApplication>

#include <okular/core/page.h>

#include "pdfium_utils.h"
#include "document.h"
#include "page.h"
#include "generator_pdfium.h"

OKULAR_EXPORT_PLUGIN(PDFiumGenerator, "libokularGenerator_pdfium.json")

static QMutex pdfiumMutex;
static int libraryRefCount;

class PDFiumGeneratorPrivate : public QSharedData
{
public:
    PDFiumGeneratorPrivate();
    PDFiumGeneratorPrivate(const PDFiumGeneratorPrivate &other);
    ~PDFiumGeneratorPrivate();

    QPdfium::Document *doc {nullptr};
    FPDF_DOCUMENT pdfDoc {nullptr};
    int pagesCount {-1};
    Okular::DocumentSynopsis *synopsis {nullptr};

private:
    void initLibrary()
    {
        if (libraryRefCount == 0) {
            FPDF_LIBRARY_CONFIG config;
            config.version = 2;
            config.m_pUserFontPaths = nullptr;
            config.m_pIsolate = nullptr;
            config.m_v8EmbedderSlot = 0;
            FPDF_InitLibraryWithConfig(&config);
        }
        ++libraryRefCount;
    }

    void uninitLibrary()
    {
        if (!--libraryRefCount) {
            FPDF_DestroyLibrary();
        }
    }
};


PDFiumGeneratorPrivate::PDFiumGeneratorPrivate()
  : pdfDoc(nullptr)
  , synopsis(nullptr)
{
    QMutexLocker lock(&pdfiumMutex);
    initLibrary();
}

PDFiumGeneratorPrivate::PDFiumGeneratorPrivate(const PDFiumGeneratorPrivate &other)
  : QSharedData(other)
  , pdfDoc(other.pdfDoc)
  , synopsis(other.synopsis)
{
    QMutexLocker lock(&pdfiumMutex);
    initLibrary();
}

PDFiumGeneratorPrivate::~PDFiumGeneratorPrivate()
{
    QMutexLocker lock(&pdfiumMutex);
    if (synopsis) {
        delete synopsis;
        synopsis = nullptr;
    }
    if (doc) {
        delete doc;
        doc = nullptr;
    }
    uninitLibrary();
}


PDFiumGenerator::PDFiumGenerator(QObject* parent, const QVariantList& args)
  : Okular::Generator(parent, args)
  , d(new PDFiumGeneratorPrivate())
{
    setFeature(Threaded);
    setFeature(TextExtraction);
    //setDPI(QSizeF(300.f,300.f));
}

PDFiumGenerator::~PDFiumGenerator()
{
}

Okular::Document::OpenResult PDFiumGenerator::loadDocumentWithPassword(const QString &fileName, QVector<Okular::Page*> &pagesVector, const QString &password)
{
    if (d->doc) {
        qDebug() << "PDFGenerator: multiple calls to loadDocument. Check it.";
        return Okular::Document::OpenError;
    }

    d->doc = QPdfium::Document::load(fileName, password);

    return init(pagesVector, password);
}

Okular::Document::OpenResult PDFiumGenerator::init(QVector<Okular::Page*> & pagesVector, const QString &password)
{
    if (!d->doc)
        return Okular::Document::OpenError;

    if (d->doc->isLocked()) {
        d->doc->unlock(password.toLatin1());
        if (d->doc->isLocked()) {
            delete d->doc;
            d->doc = nullptr;
            return Okular::Document::OpenNeedsPassword;
        }
    }

    int pageCount = d->doc->pagesCount();
    if (pageCount < 0) {
        delete d->doc;
        d->doc = nullptr;
        return Okular::Document::OpenError;
    }

    loadPages(pagesVector, 0, false);

    return Okular::Document::OpenSuccess;
}

void PDFiumGenerator::loadPages(QVector<Okular::Page*> &pagesVector, int rotation, bool clear)
{
    Q_UNUSED(rotation)
    Q_UNUSED(clear)
    QMutexLocker locker(userMutex());
    for (int pageIdx = 0; pageIdx < d->doc->pagesCount(); ++pageIdx) {
        auto page = d->doc->page(pageIdx);
        Okular::Page* okularPage = new Okular::Page(pageIdx, page->size().width(), page->size().height(), page->orientation());
        okularPage->setLabel(page->label());
        okularPage->setObjectRects(page->links());
        pagesVector.append(okularPage);
    }
}

QImage PDFiumGenerator::image(Okular::PixmapRequest* request)
{
    QMutexLocker locker(userMutex());
    Okular::Page* okularPage = request->page();
    auto page = d->doc->page(okularPage->number());
    return page->image(request->width(), request->height());
}

bool PDFiumGenerator::doCloseDocument()
{
    QMutexLocker locker(userMutex());
    if (d->doc) {
        delete d->doc;
        d->doc = nullptr;
    }
    if (d->synopsis) {
        delete d->synopsis;
        d->synopsis = nullptr;
    }
    return true;
}

Okular::DocumentInfo PDFiumGenerator::generateDocumentInfo(const QSet<Okular::DocumentInfo::Key>& keys) const
{
    Okular::DocumentInfo docInfo;
    docInfo.set(Okular::DocumentInfo::MimeType, QStringLiteral("application/pdf"));

    QMutexLocker locker(userMutex());
    if (d->doc) {
#define SET(key, val) if (keys.contains(key)) { docInfo.set(key, val); }
        SET(Okular::DocumentInfo::Title, d->doc->metaText("Title"));
        SET(Okular::DocumentInfo::Subject, d->doc->metaText("Subject"));
        SET(Okular::DocumentInfo::Author, d->doc->metaText("Author"));
        SET(Okular::DocumentInfo::Keywords, d->doc->metaText("Keywords"));
        SET(Okular::DocumentInfo::Creator, d->doc->metaText("Creator"));
        SET(Okular::DocumentInfo::Producer, d->doc->metaText("Producer"));

        auto dateTime = QPdfium::pdfiumDateToQDateTime(d->doc->metaText("CreationDate"));
        SET(Okular::DocumentInfo::CreationDate, QLocale().toString(dateTime, QLocale::LongFormat));

        dateTime = QPdfium::pdfiumDateToQDateTime(d->doc->metaText("ModDate"));
        SET(Okular::DocumentInfo::ModificationDate, QLocale().toString(dateTime, QLocale::LongFormat));
#undef SET        
        docInfo.set(Okular::DocumentInfo::Pages, QString::number(d->doc->pagesCount()));
    }
    return docInfo;
}

static void recurseCreateTOC(const FPDF_DOCUMENT &document, QDomDocument &mainDoc, 
                             FPDF_BOOKMARK parentBookmark,
                             QDomNode &parentDestination, const QSizeF &dpi)
{
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(document, parentBookmark);
    while (bookmark) {
        const unsigned long titleLength = FPDFBookmark_GetTitle(bookmark, nullptr, 0);
        QVector<ushort> titleBuffer(titleLength);
        FPDFBookmark_GetTitle(bookmark, titleBuffer.data(), titleBuffer.length());
        const FPDF_DEST destination = FPDFBookmark_GetDest(document, bookmark);
        const int pageNumber = FPDFDest_GetDestPageIndex(document, destination);

        QDomElement newel = mainDoc.createElement(QString::fromUtf16(titleBuffer.data()));
        double pageWidth, pageHeight;
        if (pageNumber != -1 && FPDF_GetPageSizeByIndex(document, pageNumber, &pageWidth, &pageHeight)) {
            Okular::DocumentViewport vp(pageNumber);
            QPointF targetPointF = QPdfium::GetLocationInPage(destination);
            if (!targetPointF.isNull()) {
                auto targetSizeF = QPdfium::GetPageSizeF(document, pageNumber);
                vp.rePos.pos = Okular::DocumentViewport::TopLeft;
                vp.rePos.normalizedX = targetPointF.x() / targetSizeF.width();;
                vp.rePos.normalizedY = (targetSizeF.height() - targetPointF.y()) / targetSizeF.height();
                vp.rePos.enabled = true;
            }
            if (parentBookmark == nullptr) {
                newel.setAttribute(QStringLiteral("Open"), QStringLiteral("true"));
            }
            newel.setAttribute(QStringLiteral("Viewport"), vp.toString());
        }
        parentDestination.appendChild(newel);
        recurseCreateTOC(document, mainDoc, bookmark, newel, dpi);
        bookmark = FPDFBookmark_GetNextSibling(document, bookmark);
    }
}

const Okular::DocumentSynopsis *PDFiumGenerator::generateDocumentSynopsis()
{
    QMutexLocker locker(userMutex());

    if (d->synopsis)
        return d->synopsis;

    d->synopsis = new Okular::DocumentSynopsis();
    recurseCreateTOC(d->doc->pdfdoc(), *d->synopsis, nullptr, *d->synopsis, dpi());

    return d->synopsis;
}

Okular::TextPage* PDFiumGenerator::textPage(Okular::TextRequest *request)
{
    QMutexLocker locker(userMutex());

    Okular::TextPage* result = new Okular::TextPage;

    auto page = d->doc->page(request->page()->number());
    auto pageWidth  = page->size().width();
    auto pageHeight = page->size().height();
    auto entityList = page->charEntityList();

    foreach (QPdfium::CharEntity *entity, entityList) {
        result->append(entity->str, new Okular::NormalizedRect(entity->area, pageWidth, pageHeight));
    }

    return result;
}

QVariant PDFiumGenerator::metaData(const QString& key, const QVariant& option) const
{
    Q_UNUSED(option);

    if (key == QLatin1String("StartFullScreen")) {
        QMutexLocker locker(userMutex());
        return d->doc->pageMode() == QPdfium::PageMode_FullScreen;
    }
    else if (key == QStringLiteral("NamedViewport") && !option.toString().isEmpty()) {
        QString optionString = option.toString();
        Okular::DocumentViewport viewport;
        auto doc = d->doc->pdfdoc();

        QMutexLocker locker(userMutex());
        FPDF_DEST destination = FPDF_GetNamedDestByName(doc, optionString.toLatin1().constData());
        const int pageNumber = FPDFDest_GetDestPageIndex(doc, destination);
        if (pageNumber >= 0) {
            QPointF targetPointF = QPdfium::GetLocationInPage(destination);
            if (!targetPointF.isNull()) {
                auto targetSizeF = QPdfium::GetPageSizeF(doc, pageNumber);
                viewport.rePos.pos = Okular::DocumentViewport::TopLeft;
                viewport.pageNumber = pageNumber;
                viewport.rePos.normalizedX = targetPointF.x() / targetSizeF.width();
                viewport.rePos.normalizedY = (targetSizeF.height() - targetPointF.y()) / targetSizeF.height();
                viewport.rePos.enabled = true;
            }
            return viewport.toString();
        }
    }
    else if (key == QLatin1String("DocumentTitle")) {
        QMutexLocker locker(userMutex());
        return d->doc->metaText("Title");
    }
    else if (key == QLatin1String("OpenTOC")) {
        QMutexLocker locker(userMutex());
        return d->doc->pageMode() == QPdfium::PageMode_UseOutlines;
    }
    return QVariant();
}

#include "generator_pdfium.moc"
