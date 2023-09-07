/*
 * modsettings.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#ifndef MODSETTINGS_H
#define MODSETTINGS_H

#include "abstractsettings.h"

namespace Ui {
class ModSettings;
}

class ModSettings : public AbstractSettings
{
	Q_OBJECT

public:
	explicit ModSettings(QWidget *parent = nullptr);
	~ModSettings();

	void initialize(const CMap & map) override;
	void update(CMap & map) override;

private slots:
	void on_modResolution_map_clicked();

	void on_modResolution_full_clicked();

	void on_treeMods_itemChanged(QTreeWidgetItem *item, int column);

private:
	void updateModWidgetBasedOnMods(const ModCompatibilityInfo & mods);

private:
	Ui::ModSettings *ui;
	const CMap * mapPointer = nullptr;
};

#endif // MODSETTINGS_H
