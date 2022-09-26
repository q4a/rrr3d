//Есть идея ввести специальные методы для сохранения прокси координат, но лучше этого не делать, поскольку пропадает прозрачность действий и существует двойственность когда прокси данные хотелось бы сделать обычными

#ifndef LSL_SERIALIZABLE
#define LSL_SERIALIZABLE

#include "lslCommon.h"
#include "lslUtility.h"
#include "lslException.h"

namespace lsl
{

class SWriter;
class SReader;
class SerialNode;
class Component;
class CollectionItem;

class Serializable
{
	friend SWriter;
	friend SReader;
	friend SerialNode;
public:
	struct FixUpName
	{
		//Имя ссылки(или иначе идентификационная строка, имя узла)
		std::string name;
		//Абсолютный путь до компонента в иерархии компонентов включая его имя
		std::string path;
		//Имя элемента коллекции
		std::string nameCollItem;

		//Приминающий FixUp Serializable(или иначе Serializable, который считывает ссылку)
		Serializable* target;
		//Узел из которого восст. ссылка
		SReader* sender;
		//Искомый компонент, по концепции поле обяз. не нулевое
		Component* component;
		//Искомый элемент коллекции, по концепции поле также ненулевое
		CollectionItem* collItem;

		template<class _Class> _Class GetComponent() const
		{
			return StaticCast<_Class>(component);
		}
		template<class _Class> _Class GetCollItem() const
		{
			return StaticCast<_Class>(collItem);
		}

		bool IsValid(bool collItem) const
		{
			return (!collItem || !nameCollItem.empty()) && !path.empty() && !name.empty();
		}
	};
	typedef std::list<FixUpName> FixUpNames;

	typedef std::list<Serializable*> ProxySerList;

	static const char* cInclude;
	static const char* cMaster;
	static const char* cItem;
private:
	Serializable* _masterSer;
	ProxySerList _proxySerList;
	unsigned _lockCnt;
protected:
	virtual void Save(SWriter* writer) = 0;
	virtual void Load(SReader* reader) = 0;
	//По концепции вызвается только один раз для серии и если есть необходимость фикс-упа
	//Событие отложенного восст. ссылок(т.е. после окончания загрузки), по концепции аргумент - список содержащий только успешно восстановленные ссылки
	virtual void OnFixUp(const FixUpNames& fixUpNames) {};
public:
	Serializable();
	~Serializable();

	//Копирование данных из источника value. Осуществляется через сериализацию. Если используются прокси ссылки, то см. пункт ниже (proxyRef)
	void AssignFromSNode(SerialNode* node);
	void AssignFromSer(Serializable* value, lsl::Component* root);

	//Прокси ссылки между Serializable для уменьшения объема сохраняемой информации. Возможны в пределах всей базы данных если происходит цельная её сериализация, иначе только внутри заголовочного SerialNode(а следовательно и Serializable), т.е. того с которого начинается сохранение и который является владельцем для всех остальных узлов. Если ссылка не может быть разрешена то происходит сохранение вместо пути тега "include" а также данных master. При загрузке используются эти данные, а тег "include" обозначает что следует оставить текущего master.
	void InsertProxySer(Serializable* value);
	void RemoveProxySer(Serializable* value);
	void ClearProxySer();
	void SyncProxySer(lsl::Component* root);
	//Мастер объект
	Serializable* GetMasterSer();
	void SetMasterSer(Serializable* value);
	//Список прокси объектов
	const ProxySerList& GetProxySerList() const;
};

}

#endif