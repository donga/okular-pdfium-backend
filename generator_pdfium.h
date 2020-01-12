/***************************************************************************
 *   Copyright (C) 2019-2020 by Thanomsub Noppaburana <donga.nb@gmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef GENERATOR_PDFIUM_H
#define GENERATOR_PDFIUM_H

#include <QScopedPointer>

#include <okular/core/document.h>
#include <okular/core/generator.h>

class PDFiumGeneratorPrivate;
class PDFiumGenerator : public Okular::Generator
{
    Q_OBJECT
    Q_INTERFACES(Okular::Generator)
public:
    PDFiumGenerator(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~PDFiumGenerator();

    Okular::Document::OpenResult loadDocumentWithPassword(const QString &fileName, QVector<Okular::Page*> &pagesVector, const QString &password) override;
    void loadPages(QVector<Okular::Page*> &pagesVector, int rotation=-1, bool clear=false);
    Okular::DocumentInfo generateDocumentInfo(const QSet<Okular::DocumentInfo::Key> &keys) const override;
    const Okular::DocumentSynopsis *generateDocumentSynopsis() override;
    QVariant metaData(const QString &key, const QVariant &option) const override;
    
protected:
    bool doCloseDocument() override;
    QImage image(Okular::PixmapRequest *page) override;
    Okular::TextPage *textPage(Okular::TextRequest *request) override;

private:
    Okular::Document::OpenResult init(QVector<Okular::Page*> & pagesVector, const QString &password);

private:
    QScopedPointer<PDFiumGeneratorPrivate> d;
};

#endif // GENERATOR_PDFIUM_H
