//Данная система устарела поскольку слишком сложна и ресурсоемка. Можно использовать упрощенный аналог (без Begin/EndUpdate и т.д.) для уведомления слушателей

#ifndef LSL_UPDATE_ABLE
#define LSL_UPDATE_ABLE

#include "lslCommon.h"

namespace lsl
{

//Стиль программирования, если обновляются сразу много полей, но для оптимизации обновления данных зависящих от значения этих полей нужно заключать вызовы методов в блок (Begin/End)Update, обновления состяние объекта будет произвделено только один раз после вызова EndUpdate
//в период отложенного обновления, все посылаемые события группируются по сендерам и внутри группы объеденяются друг с другом
class UpdateAble
{
public:
	struct UpdateData;
private:
	typedef std::map<UpdateAble*, UpdateData*> _UpdateMap;
public:
	//допущение: аргумент Unite того же типа, что и сам класс метода
	struct UpdateData 
	{
		static unsigned GetUniqueId();

		virtual ~UpdateData();

		//создает в куче полный клон объекта
		virtual UpdateData* Clone() const = 0;
		//операция объеденения
		virtual void Unite(const UpdateData& value) = 0;
		//Дополнительная идентификация типа или отдельного объекта, в случае если идентификация по sender затруднена
		virtual unsigned GetId() const
		{
			return 0;
		}
	};	
private:
	unsigned _updateCount;
	_UpdateMap _updateMap;	
protected:
	//Обработка принятых сообщений от внешних отправителей (возможно и от самого себя), здесь же может происходить отправление(или пересылка) сообщений новым приемникам.
	virtual void DoNotifyChanged(UpdateAble* sender, const UpdateData* data) = 0;
public:
	UpdateAble();

	//Уведомление об уже ПРОИСШЕДШЕМ событии
	void NotifyChanged(UpdateAble* sender, const UpdateData* data);
	void BeginUpdate();
	void EndUpdate();
	virtual bool IsUpdating() const;
};

class UpdateAbleOwned: public virtual UpdateAble
{
private:
	UpdateAble* _owner;
protected:
	virtual void DoNotifyChanged(UpdateAble* sender, const UpdateData* data);

	UpdateAble* GetOwner();
	void SetOwner(UpdateAble* value);
public:
	UpdateAbleOwned(UpdateAble* owner);
};

}

#endif