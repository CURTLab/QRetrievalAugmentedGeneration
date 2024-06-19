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

#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QtSql>
#include <QObject>
#include <QSqlDatabase>

struct Document {
	QString id;
	QString text;
	int index = 0;
	double value = 0.0;
};

class EmbeddingDatabase : public QObject
{
	Q_OBJECT
public:
	EmbeddingDatabase(QObject *parent = nullptr);

	void addCollection(const QString& collection);
	bool hasCollection(const QString& collection);
	QStringList collections();
	QString collectionByIndex(int index);

	void addDocument(const QString& id, const QString& topic, const QVector<double>& embedding);
	bool removeDocument(const QString& id);

	QVector<Document> findDocuments(const QVector<double>& targetEmbedding, int topk = 5);

	std::optional<Document> documentByIndex(int index);

signals:
	void error(const QString& message);

private:
	double calculateSimilarity(const QVector<double>& embedding1, const QVector<double>& embedding2);

	bool createConnection();
	void createTables();

	QSqlDatabase m_db;
};

#endif // EMBEDDINGDATABASE_H
