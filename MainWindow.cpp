/* MIT License
 *
 * Copyright (c) 2024 CURTLab, Fabian Hauser
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "MainWindow.h"
#include "./ui_MainWindow.h"

#include <QtPdf/QPdfDocument>
#include <QThreadPool>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_ui(new Ui::MainWindow)
	, m_bar(new QProgressBar(this))
{
	m_ui->setupUi(this);
	m_ui->statusbar->addPermanentWidget(m_bar);
	m_bar->setVisible(false);

	// Connect signals and slots
	connect(m_ui->buttonSend, &QPushButton::clicked, this, &MainWindow::sendPrompt);
	connect(m_ui->editQuestion, &QLineEdit::returnPressed, this, &MainWindow::sendPrompt);
	connect(&m_client, &OllamaClient::tokenReceived, this, &MainWindow::tokenReceived);
	connect(&m_client, &OllamaClient::finishedPrompt, this, &MainWindow::finishedPrompt);

	connect(m_ui->chat, &QTextBrowser::anchorClicked, this, &MainWindow::linkClicked);

	connect(&m_db, &EmbeddingDatabase::error, this, [this](const QString& message) {
		m_ui->statusbar->showMessage(message);
		QMessageBox::critical(this, "DB Error", message);
	});

	connect(&m_client, &OllamaClient::error, this, [this](const QString& message) {
		m_ui->statusbar->showMessage(message);
		QMessageBox::critical(this, "Ollama Error", message);
	});

	m_ui->buttonSend->setEnabled(false);
	m_ui->editQuestion->setEnabled(false);

	// Load the documents
	QTimer::singleShot(0, this, [this]() {
		std::unordered_map<QString,QVector<Document>> documents;

		m_bar->setVisible(true);
		m_bar->setMaximum(0);
		m_ui->statusbar->showMessage("Loading documents ...");

		QPdfDocument pdf;

		QDir dir("data");
		if (!dir.exists()) {
			dir.mkpath(".");
			QMessageBox::warning(this, "Warning", "No data directory found. Please add PDF files to the data directory.");
		}

		int totalChunks = 0;
		for (const auto& file : dir.entryInfoList(QDir::Files)) {
			pdf.load(file.absoluteFilePath());

			m_ui->documents->addTopLevelItem(new QTreeWidgetItem({ file.fileName(), QString::number(pdf.pageCount()) }));

			if (m_db.hasCollection(file.absoluteFilePath()))
				continue;

			QVector<Document> docs;

			// Split the document into chunks with overlap
			QString text;
			for (int i = 0; i < pdf.pageCount(); ++i) {
				// Parse page
				QString page = pdf.getAllText(i).text();
				// remove 0xEFBFBE
				page = page.replace("\xEF\xBF\xBE", "");
				page = page.replace("\r\n", "\n");
				page = page.replace(" \n", "\n");
				Q_ASSERT(!page.contains("\xEF\xBF\xBE"));

				text += page;
				int chunk = 0;

				int index = -1;
				while (text.length() > m_minTextChunk) {
					QString id = QString("%1:%2:%3").arg(file.fileName()).arg(i+1).arg(chunk);

					// Split the text into chunks at whitespace and therefore avoid cutting words in half
					index = text.indexOf(' ', m_minTextChunk);
					if (index == -1)
						break;

					docs.push_back({ id, text.left(index), -1, 0.0 });

					// Split the text at the first whitespace after the min text chunk with overlap
					index = text.indexOf(' ', m_minTextChunk - m_textOverlap);
					if (index == -1)
						index = text.indexOf(' ', m_minTextChunk / 2);
					Q_ASSERT(index != -1);

					text = text.mid(index + 1);
					++chunk;
				}

				qApp->processEvents();
			}

			// Add the last chunk
			QString id = QString("%1:%2:%3").arg(file.fileName()).arg(pdf.pageCount()).arg(documents.size());
			docs.push_back({ id, text, -1, 0.0 });

			totalChunks += docs.size();
			documents[file.absoluteFilePath()] = docs;
		}

		m_ui->statusbar->showMessage("Generating embeddings ...");

		if (!documents.empty() && totalChunks > 0) {
			m_bar->setValue(0);
			m_bar->setMaximum(totalChunks);
			for (const auto& document : documents) {
				for (const Document& doc : document.second) {
					m_db.addDocument(doc.id, doc.text, m_client.embeddingsBlocking(doc.text));
					m_bar->setValue(m_bar->value() + 1);
					qApp->processEvents();
				}
				m_db.addCollection(document.first);
			}
		}

		m_ui->buttonSend->setEnabled(true);
		m_ui->editQuestion->setEnabled(true);
		m_bar->setVisible(false);
		m_ui->statusbar->showMessage("Ready");
	});
}

MainWindow::~MainWindow()
{
}

void MainWindow::sendPrompt()
{
	m_ui->buttonSend->setEnabled(false);
	m_ui->editQuestion->setEnabled(false);

	QString question = m_ui->editQuestion->text();
	m_ui->editQuestion->clear();

	m_receivedAnswer += "**Question:** " + question + "\n\n**Answer:** ";
	m_ui->chat->setMarkdown(m_receivedAnswer);

	QVector<double> targetEmbedding = m_client.embeddingsBlocking(question);

	const int topk = 5;

	m_sources.clear();
	QString context;
	auto documents = m_db.findDocuments(targetEmbedding, topk);
	for (const Document& doc : documents) {
		context += doc.text + "\n\n";
		QString source = doc.id;
		// Remove .pdf
		source.replace(".pdf", "");
		// Remove special characters
		//source.replace(",", "");
		//source.replace(".", "");
		source.replace("-", "");
		m_sources.append(QString("[" + source + "](%1)").arg(doc.index));
	}

	m_client.setModel("mistral");
	QString prompt = m_prompTemplate.arg(context, question);
	m_client.prompt(prompt);
}

void MainWindow::tokenReceived(const QString &token)
{
	m_receivedAnswer += token;
	m_ui->chat->setMarkdown(m_receivedAnswer);
}

void MainWindow::finishedPrompt()
{
	m_ui->buttonSend->setEnabled(true);
	m_ui->editQuestion->setEnabled(true);

	m_receivedAnswer += "\n\n**Sources:** " + m_sources.join(", ") + "\n\n";

	m_ui->chat->setMarkdown(m_receivedAnswer);
}

void MainWindow::linkClicked(const QUrl &url)
{
	bool ok = false;
	int id = url.toString().toInt(&ok);

	if (ok) {
		auto doc = m_db.documentByIndex(id);
		if (doc.has_value()) {
			// open external pdf viewer
			QString file = doc->id.split(":").first();
			// Run the external viewer
			QDesktopServices::openUrl(QUrl::fromLocalFile("data/" + file));
		}
	}
}
