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

#ifndef OLLAMACLIENT_H
#define OLLAMACLIENT_H

#include <QObject>
#include <QNetworkAccessManager>

class OllamaClient : public QObject
{
	Q_OBJECT
public:
	explicit OllamaClient(QObject *parent = nullptr);

	void prompt(const QString &text);
	QVector<double> embeddingsBlocking(const QString &text);
	QString promptBlocking(const QString &text);

signals:
	void tokenReceived(const QString &token);
	void finishedPrompt();
	void newSession();

	void error(const QString& message);

public slots:
	void setModel(const QString &model);
	void clearHistory();

private slots:
	void replyReadyRead();

private:
	QNetworkAccessManager *m_manager;
	QNetworkReply *m_reply;
	QString m_model = "llama3";
	QString m_chatHistory;

};

#endif // OLLAMACLIENT_H
