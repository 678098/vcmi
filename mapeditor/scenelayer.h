#ifndef SCENELAYER_H
#define SCENELAYER_H

#include "../lib/int3.h"

class MapSceneBase;
class MapScene;
class CGObjectInstance;
class MapController;
class CMap;
class MapHandler;

class AbstractLayer : public QObject
{
	Q_OBJECT
public:
	AbstractLayer(MapSceneBase * s);
	
	virtual void update() = 0;
	
	void show(bool show);
	void redraw();
	void initialize(MapController & controller);
	
protected:
	MapSceneBase * scene;
	CMap * map = nullptr;
	MapHandler * handler = nullptr;
	bool isShown = false;
	
	std::unique_ptr<QPixmap> pixmap;
	QPixmap emptyPixmap;
	
private:
	std::unique_ptr<QGraphicsPixmapItem> item;
};


class GridLayer: public AbstractLayer
{
	Q_OBJECT
public:
	GridLayer(MapSceneBase * s);
	
	void update() override;
};

class PassabilityLayer: public AbstractLayer
{
	Q_OBJECT
public:
	PassabilityLayer(MapSceneBase * s);
	
	void update() override;
};

class SelectionTerrainLayer: public AbstractLayer
{
	Q_OBJECT
public:
	SelectionTerrainLayer(MapSceneBase* s);
	
	void update() override;
	
	void draw();
	void select(const int3 & tile);
	void erase(const int3 & tile);
	void clear();
	
	const std::set<int3> & selection() const;

signals:
	void selectionMade(bool anythingSlected);

private:
	std::set<int3> area, areaAdd, areaErase;

	void onSelection();
};


class TerrainLayer: public AbstractLayer
{
	Q_OBJECT
public:
	TerrainLayer(MapSceneBase * s);
	
	void update() override;
	
	void draw(bool onlyDirty = true);
	void setDirty(const int3 & tile);
	
private:
	std::set<int3> dirty;
};


class ObjectsLayer: public AbstractLayer
{
	Q_OBJECT
public:
	ObjectsLayer(MapSceneBase * s);
	
	void update() override;
	
	void draw(bool onlyDirty = true); //TODO: implement dirty
	
	void setDirty(int x, int y);
	void setDirty(const CGObjectInstance * object);
	
private:
	std::set<const CGObjectInstance *> dirty;
};


class SelectionObjectsLayer: public AbstractLayer
{
	Q_OBJECT
public:
	SelectionObjectsLayer(MapSceneBase* s);
	
	void update() override;
	
	void draw();
	
	CGObjectInstance * selectObjectAt(int x, int y) const;
	void selectObjects(int x1, int y1, int x2, int y2);
	void selectObject(CGObjectInstance *, bool inform = true);
	bool isSelected(const CGObjectInstance *) const;
	std::set<CGObjectInstance*> getSelection() const;
	void moveSelection(int x, int y);
	void clear();
		
	QPoint shift;
	CGObjectInstance * newObject;
	//FIXME: magic number
	int selectionMode = 0; //0 - nothing, 1 - selection, 2 - movement

signals:
	void selectionMade(bool anythingSlected);
	
private:
	std::set<CGObjectInstance *> selectedObjects;

	void onSelection();
};

class MinimapLayer: public AbstractLayer
{
public:
	MinimapLayer(MapSceneBase * s);
	
	void update() override;
};

class MinimapViewLayer: public AbstractLayer
{
public:
	MinimapViewLayer(MapSceneBase * s);
	
	void setViewport(int x, int y, int w, int h);
	
	void draw();
	void update() override;
	
	int viewportX() const {return x;}
	int viewportY() const {return y;}
	int viewportWidth() const {return w;}
	int viewportHeight() const {return h;}
	
private:
	int x = 0, y = 0, w = 1, h = 1;
	
};

#endif // SCENELAYER_H
