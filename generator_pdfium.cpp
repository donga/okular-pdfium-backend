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
#include <QBitArray>
#include <QLocale>
#include <QDateTime>
#include <QImage>
#include <QTimer>
#include <QMutexLocker>

#include <okular/core/action.h>
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

public:
    PDFiumGenerator *q;
    Okular::Page* *pagesVector {nullptr};
    QPdfium::Document *doc {nullptr};
    int pagesCount {-1};
    Okular::DocumentSynopsis *synopsis {nullptr};
    QBitArray rectsGenerated;

public:
    bool fillDocumentViewport(FPDF_DEST destination, Okular::DocumentViewport *viewport)
    {
        bool result = false;
        
        if (!doc || !viewport)
            return result;
        
        if (!doc->pdfdoc())
            return result;
        
        const int pageNumber = FPDFDest_GetDestPageIndex(doc->pdfdoc(), destination);
        if (pageNumber >= 0) {
            viewport->pageNumber = pageNumber;
            
            QPointF targetPointF = QPdfium::GetLocationInPage(destination);
            if (!targetPointF.isNull()) {
                auto targetSizeF = QPdfium::GetPageSizeF(doc->pdfdoc(), pageNumber);
                viewport->rePos.pos = Okular::DocumentViewport::TopLeft;
                viewport->rePos.normalizedX = targetPointF.x() / targetSizeF.width();
                viewport->rePos.normalizedY = (targetSizeF.height() - targetPointF.y()) / targetSizeF.height();
                viewport->rePos.enabled = true;
            }
            result = true;
        }
        
        return result;
    }

    void recurseCreateTOC(QDomDocument &mainDoc, 
                          FPDF_BOOKMARK parentBookmark,
                          QDomNode &parentDestination)
    {
        FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(doc->pdfdoc(), parentBookmark);
        while (bookmark) {
            QDomElement newel = mainDoc.createElement(QPdfium::GetBookmarkTitle(bookmark));
            
            Okular::DocumentViewport viewport;
            if (fillDocumentViewport(FPDFBookmark_GetDest(doc->pdfdoc(), bookmark), &viewport)) {
                if (parentBookmark == nullptr) {
                    newel.setAttribute(QStringLiteral("Open"), QStringLiteral("true"));
                }
                newel.setAttribute(QStringLiteral("Viewport"), viewport.toString());
            }
            
            parentDestination.appendChild(newel);
            recurseCreateTOC(mainDoc, bookmark, newel);
            bookmark = FPDFBookmark_GetNextSibling(doc->pdfdoc(), bookmark);
        }
    }
    
    Okular::Page *newOkularPage(int pageNumber, Okular::Rotation orientation, const QSizeF &dpi)
    {
        auto pageSize = QPdfium::GetPageSizeF(doc->pdfdoc(), pageNumber);
        pageSize.setWidth(pageSize.width() / 72.0 * dpi.width());
        pageSize.setHeight(pageSize.height() / 72.0 * dpi.height());

        Okular::Page* newPage = new Okular::Page(pageNumber, pageSize.width(), pageSize.height(), orientation);
        newPage->setLabel(QPdfium::GetPageLabel(doc->pdfdoc(), pageNumber));
        
        return newPage;
    }

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
  : synopsis(nullptr)
{
    QMutexLocker lock(&pdfiumMutex);
    initLibrary();
}

PDFiumGeneratorPrivate::PDFiumGeneratorPrivate(const PDFiumGeneratorPrivate &other)
  : QSharedData(other)
  , synopsis(other.synopsis)
{
    QMutexLocker lock(&pdfiumMutex);
    initLibrary();
}

PDFiumGeneratorPrivate::~PDFiumGeneratorPrivate()
{
    QMutexLocker lock(&pdfiumMutex);
    
    pagesVector = nullptr;
    
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
    d->q = this;
    setFeature(Threaded);
    setFeature(TextExtraction);
    //setFeature(PageSizes);
    setFeature(TiledRendering);
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

    d->doc = QPdfium::Document::load(fileName, password, dpi());

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
    
    d->rectsGenerated.fill(false, pageCount);
    pagesVector.resize(pageCount);
    loadPages(pagesVector, 0, false);

    return Okular::Document::OpenSuccess;
}

void PDFiumGenerator::loadPages(QVector<Okular::Page*> &pagesVector, int rotation, bool clear)
{
    Q_UNUSED(rotation)
    Q_UNUSED(clear)
    
    QMutexLocker locker(userMutex());
    
    d->pagesVector = pagesVector.data();
    
    for (int pageNumber = 0; pageNumber < d->doc->pagesCount(); ++pageNumber) {
        d->pagesVector[pageNumber] = d->newOkularPage(pageNumber, Okular::Rotation0, dpi());
    }
}


struct RenderImagePayload
{
    RenderImagePayload(PDFiumGenerator *g, Okular::PixmapRequest *r) :
        generator(g), request(r)
    {
        // Don't report partial updates for the first 500 ms
        timer.setInterval(500);
        timer.setSingleShot(true);
        timer.start();
    }

    PDFiumGenerator *generator;
    Okular::PixmapRequest *request;
    QTimer timer;
};
Q_DECLARE_METATYPE(RenderImagePayload*)


QImage PDFiumGenerator::image(Okular::PixmapRequest* request)
{
    // compute dpi used to get an image with desired width and height
    Okular::Page *okularPage = request->page();

    double pageWidth = okularPage->width(),
           pageHeight = okularPage->height();

    if ( okularPage->rotation() % 2 )
        qSwap( pageWidth, pageHeight );

    float fakeDpiX = request->width() / pageWidth * dpi().width();
    float fakeDpiY = request->height() / pageHeight * dpi().height();
    
    QMutexLocker locker(userMutex());
    
    const int pageNumber = request->pageNumber();
    auto page = d->doc->page(pageNumber);
    
    if (request->shouldAbortRender()) {
        return QImage();
    }
    
    if (page) {
        request->page()->text(); // call text for trigger generate ObjectRects ??
        
        if (request->isTile()) {
            const QRect rect = request->normalizedRect().geometry( request->width(), request->height() );
            /*if (request->partialUpdatesWanted()) {
                //RenderImagePayload payload( this, request );
                //img = p->renderToImage( fakeDpiX, fakeDpiY, rect.x(), rect.y(), rect.width(), rect.height(), Poppler::Page::Rotate0,
                //                        partialUpdateCallback, shouldDoPartialUpdateCallback, shouldAbortRenderCallback, QVariant::fromValue( &payload ) );
                qDebug() << "image()->tile():partialUpdatesWanted() reqRect:" << rect 
                         << " fakeDpi: " << QSizeF(fakeDpiX,fakeDpiY) 
                         << " pageSize:" << page->size() 
                         << " reqSize: " << QSizeF(request->width(), request->height());
                return page->renderToImage(fakeDpiX, fakeDpiY, rect.x(), rect.y(), rect.width(), rect.height(), Okular::Rotation0);
            }
            else {
                //RenderImagePayload payload( this, request );
                //img = p->renderToImage( fakeDpiX, fakeDpiY, rect.x(), rect.y(), rect.width(), rect.height(), Poppler::Page::Rotate0,
                //                        nullptr, nullptr, shouldAbortRenderCallback, QVariant::fromValue( &payload ) );
                qDebug() << "image()->tile():!partialUpdatesWanted()" << rect << QSizeF(fakeDpiX,fakeDpiY) << page->size() << QSizeF(request->width(), request->height());
                return page->renderToImage(fakeDpiX, fakeDpiY, rect.x(), rect.y(), rect.width(), rect.height(), Okular::Rotation0);
            }*/
            return page->renderToImage(fakeDpiX, fakeDpiY, rect.x(), rect.y(), rect.width(), rect.height(), Okular::Rotation0);
        }
        else {
            return page->image(request->width(), request->height());
        }
    }
    
    return QImage();
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
    d->rectsGenerated.clear();
    
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

const Okular::DocumentSynopsis *PDFiumGenerator::generateDocumentSynopsis()
{
    if (d->synopsis) {
        return d->synopsis;
    }

    QMutexLocker locker(userMutex());
    
    d->synopsis = new Okular::DocumentSynopsis();
    d->recurseCreateTOC(*d->synopsis, nullptr, *d->synopsis);

    return d->synopsis;
}

Okular::TextPage* PDFiumGenerator::textPage(Okular::TextRequest *request)
{
    const int pageNumber = request->page()->number();
    Okular::TextPage* result = new Okular::TextPage;

    QMutexLocker locker(userMutex());
    
    auto page = d->doc->page(pageNumber);
    if (page) {
        auto pageWidth  = page->size().width();
        auto pageHeight = page->size().height();
        auto entityList = page->charEntityList();
    
        foreach (QPdfium::CharEntity *entity, entityList) {
            result->append(entity->str, new Okular::NormalizedRect(entity->area, pageWidth, pageHeight));
        }

        // generate links rects & change page orientation only the first time
        bool genObjectRects = !d->rectsGenerated.at(pageNumber);
        if (genObjectRects) {
            if (page->hasLinks()) {
                request->page()->setObjectRects(page->links());
            }

            // Change page orientation
            if (request->page()->orientation() != page->orientation()) {
                auto oldPage = d->pagesVector[pageNumber];
                d->pagesVector[pageNumber] = d->newOkularPage(pageNumber, page->orientation(), dpi());
                delete oldPage;
            }
        }
        d->rectsGenerated[pageNumber] = true;
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
        QString optionStr = option.toString();
        Okular::DocumentViewport viewport;

        QMutexLocker locker(userMutex());
        FPDF_DEST dest = FPDF_GetNamedDestByName(d->doc->pdfdoc(), optionStr.toLatin1().constData());
        if (d->fillDocumentViewport(dest, &viewport))
            return viewport.toString();
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
