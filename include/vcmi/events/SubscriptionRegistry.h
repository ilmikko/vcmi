/*
 * SubscriptionRegistry.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

class Environment;

namespace events
{

class EventBus;

class DLL_LINKAGE EventSubscription : public boost::noncopyable
{
public:
	virtual ~EventSubscription() = default;
};

template <typename E>
class SubscriptionRegistry : public boost::noncopyable
{
public:
	using PreHandler = std::function<void(const Environment *, const EventBus *, E &)>;
	using PostHandler = std::function<void(const Environment *, const EventBus *, const E &)>;
	using BusTag = const void *;

	std::unique_ptr<EventSubscription> subscribeBefore(BusTag tag, PreHandler && cb)
	{
		boost::unique_lock<boost::shared_mutex> lock(mutex);

		auto storage = std::make_shared<PreHandlerStorage>(std::move(cb));
		preHandlers[tag].push_back(storage);
		return make_unique<PreSubscription>(tag, storage);
	}

	std::unique_ptr<EventSubscription> subscribeAfter(BusTag tag, PostHandler && cb)
	{
		boost::unique_lock<boost::shared_mutex> lock(mutex);

		auto storage = std::make_shared<PostHandlerStorage>(std::move(cb));
		postHandlers[tag].push_back(storage);
		return make_unique<PostSubscription>(tag, storage);
	}

	void executeEvent(const Environment * env, const EventBus * bus, E & event)
	{
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		{
			auto it = preHandlers.find(bus);

			if(it != std::end(preHandlers))
			{
				for(auto & h : it->second)
					(*h)(env, bus, event);
			}
		}

		event.execute(env, bus);

		{
			auto it = postHandlers.find(bus);

			if(it != std::end(postHandlers))
			{
				for(auto & h : it->second)
					(*h)(env, bus, event);
			}
		}
	}

private:

	template <typename T>
	class HandlerStorage
	{
	public:
		explicit HandlerStorage(T && cb_)
			: cb(cb_)
		{
		}

		void operator()(const Environment * env, const EventBus * bus, E & event)
		{
			cb(env, bus, event);
		}
	private:
		T cb;
	};

	using PreHandlerStorage = HandlerStorage<PreHandler>;
	using PostHandlerStorage = HandlerStorage<PostHandler>;

	class PreSubscription : public EventSubscription
	{
	public:
		PreSubscription(BusTag tag_, std::shared_ptr<PreHandlerStorage> cb_)
			: cb(cb_),
			tag(tag_)
		{
		}

		virtual ~PreSubscription()
		{
			boost::unique_lock<boost::shared_mutex> lock(E::getRegistry()->mutex);

			auto & handlers = E::getRegistry()->preHandlers;
			auto it = handlers.find(tag);

			if(it != std::end(handlers))
				it->second -= cb;
		}
	private:
		std::shared_ptr<PreHandlerStorage> cb;
		BusTag tag;
	};

	class PostSubscription : public EventSubscription
	{
	public:
		PostSubscription(BusTag tag_, std::shared_ptr<PostHandlerStorage> cb_)
			: cb(cb_),
			tag(tag_)
		{
		}

		virtual ~PostSubscription()
		{
			boost::unique_lock<boost::shared_mutex> lock(E::getRegistry()->mutex);

			auto & handlers = E::getRegistry()->postHandlers;
			auto it = handlers.find(tag);

			if(it != std::end(handlers))
				it->second -= cb;
		}
	private:
		BusTag tag;
		std::shared_ptr<PostHandlerStorage> cb;
	};

	boost::shared_mutex mutex;

	std::map<BusTag, std::vector<std::shared_ptr<PreHandlerStorage>>> preHandlers;
	std::map<BusTag, std::vector<std::shared_ptr<PostHandlerStorage>>> postHandlers;
};

}



