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

#include "EmbeddingDatabase.h"

EmbeddingDatabase::EmbeddingDatabase(QObject *parent)
	: QObject(parent)
{
	if (!createConnection()) {
		return;
	}
	createTables();
}

void EmbeddingDatabase::addCollection(const QString &collection)
{
	QSqlQuery query;
	query.prepare("INSERT INTO collections (id, name, topic) VALUES (:id, :name, :topic)");
	query.bindValue(":id", QUuid::createUuid().toString());
	query.bindValue(":name", collection);
	query.bindValue(":topic", collection);

	if (!query.exec()) {
		emit error("Error inserting collection: " + query.lastError().text());
	}
}

bool EmbeddingDatabase::hasCollection(const QString &collection)
{
	QSqlQuery query;
	query.prepare("SELECT id FROM collections WHERE name = :name");
	query.bindValue(":name", collection);

	if (!query.exec()) {
		emit error("Error selecting collection: " + query.lastError().text());
		return false;
	}

	return query.next();
}

QStringList EmbeddingDatabase::collections()
{
	QSqlQuery query;
	query.prepare("SELECT name FROM collections");

	if (!query.exec()) {
		emit error("Error selecting collections: " + query.lastError().text());
		return {};
	}

	QStringList collectionNames;
	while (query.next()) {
		collectionNames.append(query.value("name").toString());
	}

	return collectionNames;
}

QString EmbeddingDatabase::collectionByIndex(int index)
{
	QSqlQuery query;
	query.prepare("SELECT name FROM collections WHERE rowid = :index");
	query.bindValue(":index", index + 1);

	if (!query.exec()) {
		emit error("Error selecting collection: " + query.lastError().text());
		return {};
	}

	return query.next() ? query.value("name").toString() : "";
}

void EmbeddingDatabase::addDocument(const QString &id, const QString &topic, const QVector<double> &embedding)
{
	QByteArray embeddingsData(reinterpret_cast<const char*>(embedding.data()), embedding.size() * sizeof(double));

	// check if embedding already exists in the database
	QSqlQuery checkQuery;
	checkQuery.prepare("SELECT id FROM embeddings_queue WHERE id = :id OR vector = :vector");
	checkQuery.bindValue(":id", id);
	checkQuery.bindValue(":vector", embeddingsData);
	if (checkQuery.exec() && checkQuery.next()) {
		qDebug() << "Document already exists in the database";
		return;
	}

	QSqlQuery query;
	query.prepare("INSERT INTO embeddings_queue (operation, topic, id, vector) VALUES (:operation, :topic, :id, :vector)");
	query.bindValue(":operation", 1);
	query.bindValue(":topic", topic);
	query.bindValue(":id", id);
	query.bindValue(":vector", embeddingsData);

	if (!query.exec()) {
		emit error("Error inserting document: " + query.lastError().text());
	}
}

bool EmbeddingDatabase::removeDocument(const QString &id)
{
	QSqlQuery deleteQuery;
	deleteQuery.prepare("DELETE FROM embeddings_queue WHERE id = :id");
	deleteQuery.bindValue(":id", id);
	if (!deleteQuery.exec()) {
		emit error("Error deleting document: " + deleteQuery.lastError().text());
		return false;
	}
	return true;
}

QVector<Document> EmbeddingDatabase::findDocuments(const QVector<double> &targetEmbedding, int topk)
{
	QSqlQuery query;
	query.prepare("SELECT id, vector FROM embeddings_queue WHERE operation = 1");

	if (!query.exec()) {
		emit error("Error selecting documents: " + query.lastError().text());
		return {};
	}

	QVector<Document> closestDocuments;
	while (query.next()) {
		QString id = query.value("id").toString();
		const QByteArray vectorData = query.value("vector").toByteArray();
		if (vectorData.isEmpty()) {
			qWarning() << "Empty embedding for document with id" << id << query.lastError().text();
			continue;
		}

		QVector<double> embedding;
		embedding.resize(vectorData.size() / sizeof(double));
		std::memcpy(embedding.data(), vectorData.constData(), vectorData.size());

		const double similarity = calculateSimilarity(targetEmbedding, embedding);
		closestDocuments.append({id, "", -1, similarity});
	}

	// Sort documents by similarity and return the top k
	std::sort(closestDocuments.begin(), closestDocuments.end(), [](const Document& a, const Document& b) {
		return a.value > b.value;
	});
	closestDocuments = closestDocuments.mid(0, topk);

	// Populate text and other metadata
	for (Document& doc : closestDocuments) {
		QSqlQuery metadataQuery;
		metadataQuery.prepare("SELECT seq_id, topic FROM embeddings_queue WHERE id = :id");
		metadataQuery.bindValue(":id", doc.id);
		if (!metadataQuery.exec()) {
			emit error("Error selecting metadata: " + metadataQuery.lastError().text());
			continue;
		}

		if (metadataQuery.next()) {
			doc.text = metadataQuery.value("topic").toString();
			doc.index = metadataQuery.value("seq_id").toInt();
		}
	}

	return closestDocuments;
}

std::optional<Document> EmbeddingDatabase::documentByIndex(int index)
{
	QSqlQuery query;
	query.prepare("SELECT id, topic FROM embeddings_queue WHERE seq_id = :index");
	query.bindValue(":index", index);

	if (!query.exec()) {
		emit error("Error selecting document: " + query.lastError().text());
		return {};
	}

	if (query.next()) {
		Document doc;
		doc.id = query.value("id").toString();
		doc.text = query.value("topic").toString();
		doc.index = index;
		return doc;
	}

	return {};
}

double EmbeddingDatabase::calculateSimilarity(const QVector<double> &embedding1, const QVector<double> &embedding2)
{
	// Calculate cosine similarity

	// Dot product
	double dotProduct = 0.0;
	for (int i = 0; i < embedding1.size(); ++i) {
		dotProduct += embedding1[i] * embedding2[i];
	}

	// Magnitudes
	double magnitude1 = 0.0, magnitude2 = 0.0;
	for (int i = 0; i < embedding1.size(); ++i) {
		magnitude1 += embedding1[i] * embedding1[i];
		magnitude2 += embedding2[i] * embedding2[i];
	}
	magnitude1 = std::sqrt(magnitude1);
	magnitude2 = std::sqrt(magnitude2);

	// Cosine similarity
	return dotProduct / (magnitude1 * magnitude2);
}

bool EmbeddingDatabase::createConnection()
{
	m_db = QSqlDatabase::addDatabase("QSQLITE");
	m_db.setDatabaseName("embeddings.db");

	if (!m_db.open()) {
		emit error("Error opening database: " + m_db.lastError().text());
		return false;
	}

	return true;
}

void EmbeddingDatabase::createTables()
{
	// check if tables already exist
	QSqlQuery checkQuery;
	checkQuery.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name IN ('embeddings_queue', 'collections', 'collection_metadata')");
	if (checkQuery.exec() && checkQuery.next()) {
		return;
	}

	QSqlQuery query;
	// Create embeddings_queue table
	if (!query.exec("CREATE TABLE embeddings_queue ("
					"seq_id INTEGER PRIMARY KEY, "
					"created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
					"operation INTEGER NOT NULL, "
					"topic TEXT NOT NULL, "
					"id TEXT NOT NULL, "
					"vector BLOB, "
					"encoding TEXT, "
					"metadata TEXT)")) {
		emit error("Error creating embeddings_queue table: " + query.lastError().text());
	}

	// Create collections table
	if (!query.exec("CREATE TABLE collections ("
					"id TEXT PRIMARY KEY, "
					"name TEXT NOT NULL, "
					"topic TEXT NOT NULL, "
					"UNIQUE (name))")) {
		emit error("Error creating collections table: " + query.lastError().text());
	}
}
