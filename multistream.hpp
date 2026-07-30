#pragma once

#include "config-dialog.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <chrono>
#include <string>
#include <vector>

class OBSBasicSettings;

struct MultistreamOutputEntry {
	std::string name;
	obs_output_t *output = nullptr;
	QPushButton *button = nullptr;
	QLabel *statusLabel = nullptr;
	uint64_t lastBytes = 0;
	std::chrono::steady_clock::time_point lastBytesTime{};
	bool stopping = false;
	bool starting = false;
	bool wasReconnecting = false;
	bool reconnectWarningShown = false;
	std::chrono::steady_clock::time_point liveSince{};
	std::vector<std::chrono::steady_clock::time_point> recentDisconnects;
};

class MultistreamDock : public QFrame {
	Q_OBJECT

private:
	OBSBasicSettings *configDialog = nullptr;

	obs_data_t *current_config = nullptr;

	QVBoxLayout *mainLayout = nullptr;
	QVBoxLayout *mainCanvasLayout = nullptr;
	QVBoxLayout *mainCanvasOutputLayout = nullptr;
	QVBoxLayout *verticalCanvasLayout = nullptr;
	QVBoxLayout *verticalCanvasOutputLayout = nullptr;
	QPushButton *mainStreamButton = nullptr;
	QPushButton *configButton = nullptr;
	QLabel *mainPlatformIconLabel = nullptr;
	QString mainPlatformUrl;

	QString newer_version_available;
	time_t partnerBlockTime = 0;

	QTimer videoCheckTimer;
	video_t *mainVideo = nullptr;
	std::vector<video_t *> oldVideo;

	std::vector<MultistreamOutputEntry> outputs;
	obs_data_array_t *vertical_outputs = nullptr;
	bool exiting = false;
	bool finished_loading = false;

	void LoadSettingsFile();
	void LoadSettings();
	void LoadOutput(obs_data_t *data, bool vertical);
	void SaveSettings();

	bool StartOutput(obs_data_t *settings, QPushButton *streamButton);

	void outputButtonStyle(QPushButton *button);
	void UpdateOutputStatuses();
	void SetOutputStatus(MultistreamOutputEntry &entry, const QString &text, const QString &color = QString());
	void NoteReconnectAttempt(MultistreamOutputEntry &entry);
	void MaybeShowReconnectLoopWarning(MultistreamOutputEntry &entry);
	void ResetReconnectTracking(MultistreamOutputEntry &entry);
	MultistreamOutputEntry *FindOutput(const std::string &name);
	MultistreamOutputEntry *FindOutput(obs_output_t *output);

	void storeMainStreamEncoders();

	void AskUpdate();

	QIcon streamActiveIcon = QIcon(":/aitum/media/streaming.svg");
	QIcon streamInactiveIcon = QIcon(":/aitum/media/stream.svg");

	static void frontend_event(enum obs_frontend_event event, void *private_data);

	static void stream_output_stop(void *data, calldata_t *calldata);
	static void stream_output_start(void *data, calldata_t *calldata);

private slots:
	void ApiInfo(QString info);

public:
	MultistreamDock(QWidget *parent = nullptr);
	~MultistreamDock();
	void LoadVerticalOutputs(bool firstLoad = true);
};

class AspectRatioPixmapLabel : public QLabel {
	Q_OBJECT
public:
	explicit AspectRatioPixmapLabel(QWidget *parent = 0);
	virtual int heightForWidth(int width) const;
	virtual QSize sizeHint() const;
	QPixmap scaledPixmap() const;
public slots:
	void setPixmap(const QPixmap &);
	void resizeEvent(QResizeEvent *);

private:
	QPixmap pix;
};
