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

#include "OllamaClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>

// https://github.com/ollama/ollama/blob/main/docs/api.md

OllamaClient::OllamaClient(QObject *parent)
	: QObject{parent}
{
	m_manager = new QNetworkAccessManager(this);
}

void OllamaClient::prompt(const QString &text)
{
	QUrl url("http://localhost:11434/api/generate");
	//QUrl url("http://localhost:11434/api/chat");
	QNetworkRequest request(url);

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	m_chatHistory += "Prompter:" + text + "\nAI:";

	QJsonObject json;
	json["model"] = m_model;

#if 0
	QJsonArray messages;
	message["role"] = "prompter";
	message["content"] = text;
	json["messages"] = message;
#endif

	json["prompt"] = m_chatHistory;

	QJsonDocument doc(json);
	QByteArray data = doc.toJson();

	m_reply = m_manager->post(request, data);
	connect(m_reply, &QNetworkReply::readyRead, this, &OllamaClient::replyReadyRead);
}

QVector<double> OllamaClient::embeddingsBlocking(const QString &text)
{
	QUrl url("http://localhost:11434/api/embeddings");
	QNetworkRequest request(url);

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QJsonObject json;
	json["model"] = "nomic-embed-text";
	json["prompt"] = text;
	json["stream"] = false;

	QJsonDocument doc(json);
	QByteArray data = doc.toJson();

	// blocking reply
	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.post(request, data);
	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		emit error("Error in embeddingsBlocking: " + reply->errorString());
		return {};
	}

	QByteArray responseData = reply->readAll();
	QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

	if (responseDoc.object().contains("error")) {
		emit error(responseDoc["error"].toString());
		return {};
	}

	QVector<double> embeddings;

	QJsonArray arr = responseDoc["embedding"].toArray();
	embeddings.reserve(arr.size());

	for (const QJsonValue &val : arr)
		embeddings.append(val.toDouble());

	return embeddings;
}

QString OllamaClient::promptBlocking(const QString &text)
{
	QUrl url("http://localhost:11434/api/generate");
	QNetworkRequest request(url);

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QJsonObject json;
	json["model"] = m_model;
	json["prompt"] = text;
	json["stream"] = false;

	QJsonDocument doc(json);
	QByteArray data = doc.toJson();

	// blocking reply
	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.post(request, data);
	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		emit error("Error in promptBlocking: " + reply->errorString());
		return {};
	}

	QByteArray responseData = reply->readAll();
	QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

	if (responseDoc.object().contains("error")) {
		emit error(responseDoc["error"].toString());
		return "";
	}

	return responseDoc["response"].toString();
}

void OllamaClient::setModel(const QString &model)
{
	if (m_model == model)
		return;

	m_model = model;
	m_chatHistory.clear();
	emit newSession();
}

void OllamaClient::clearHistory()
{
	m_chatHistory.clear();
	emit newSession();
}

void OllamaClient::replyReadyRead()
{
	QByteArray responseData = m_reply->readAll();
	//processResponse(responseData);

	// parse {...}\n
	QString response = responseData;
	int start = response.lastIndexOf('{');
	int end = response.lastIndexOf('}');
	if (start == -1 || end == -1 || start >= end) {
		return;
	}

	response = response.mid(start, end - start + 1);
	QJsonDocument doc = QJsonDocument::fromJson(responseData);
	QJsonObject obj = doc.object();

	if (obj.contains("error")) {
		emit error(obj["error"].toString());
		return;
	}

	if (obj["done"].toBool()) {
		emit finishedPrompt();
	} else {
		QString response = obj["response"].toString();
		m_chatHistory += response;
		emit tokenReceived(response);
	}
}
