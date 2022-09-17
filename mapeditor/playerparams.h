#ifndef PLAYERPARAMS_H
#define PLAYERPARAMS_H

#include <QWidget>
#include "../lib/mapping/CMap.h"
#include "mapcontroller.h"

namespace Ui {
class PlayerParams;
}

class PlayerParams : public QWidget
{
	Q_OBJECT

public:
	explicit PlayerParams(MapController & controller, int playerId, QWidget *parent = nullptr);
	~PlayerParams();

	PlayerInfo playerInfo;
	int playerColor;

private slots:
	void on_radioHuman_toggled(bool checked);

	void on_radioCpu_toggled(bool checked);

	void on_mainTown_activated(int index);

	void on_generateHero_stateChanged(int arg1);

	void on_randomFaction_stateChanged(int arg1);
	
	void allowedFactionsCheck(QListWidgetItem *);

private:
	Ui::PlayerParams *ui;
	
	MapController & controller;
};

#endif // PLAYERPARAMS_H
