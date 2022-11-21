#ifndef LOBBYROOMREQUEST_MOC_H
#define LOBBYROOMREQUEST_MOC_H

#include <QDialog>
#include "lobby.h"

namespace Ui {
class LobbyRoomRequest;
}

class LobbyRoomRequest : public QDialog
{
	Q_OBJECT

public:
	explicit LobbyRoomRequest(SocketLobby & socket, const QString & room, const QMap<QString, QString> & mods, QWidget *parent = nullptr);
	~LobbyRoomRequest();

private slots:
	void on_buttonBox_accepted();

private:
	Ui::LobbyRoomRequest *ui;
	SocketLobby & socketLobby;
	QMap<QString, QString> mods;
};

#endif // LOBBYROOMREQUEST_MOC_H
