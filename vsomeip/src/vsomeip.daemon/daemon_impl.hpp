//
// daemon_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_DAEMON_IMPL_HPP
#define VSOMEIP_DAEMON_DAEMON_IMPL_HPP

#include <map>
#include <set>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost_ext/asio/mq.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/application_base_impl.hpp>
#include <vsomeip_internal/daemon.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>
#include <vsomeip_internal/serializer.hpp>

#include "application_info.hpp"
#include "client_info.hpp"
#include "request_info.hpp"
#include "service_info.hpp"

//#define VSOMEIP_DAEMON_DEBUG

namespace vsomeip {

class endpoint;
class client;
class service;

namespace sd {
class service_discovery;
} // namespace sd

class daemon_impl
		: public daemon,
		  public application_base_impl {
public:
	static daemon * get_instance();

	daemon_impl();
	~daemon_impl();

	boost::log::sources::severity_logger< boost::log::trivial::severity_level > & get_logger();

	client_id get_id() const;
	void set_id(client_id _id);

	std::string get_name() const;
	void set_name(const std::string &_name);

	bool is_managing() const;

	boost::asio::io_service & get_service();
	boost::asio::io_service & get_sender_service();
	boost::asio::io_service & get_receiver_service();

	void init(int _count, char **_options);
	void start();
	void stop();

	bool send(message_base *_message, bool _reliable, bool _flush);

	void catch_up_registrations();
	void on_service_availability(client_id _client, service_id _service, instance_id _instance, const endpoint *_location, bool _is_available);
	void on_subscription(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location, bool _is_subscribing);
	void on_add_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _field, const uint8_t *payload, uint32_t payload_size);

	// Dummies
	void handle_message(std::shared_ptr< message > &_message);
	void handle_availability(service_id _service, instance_id _instance, const endpoint *_location, bool _is_available);
	void handle_subscription(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location, bool _is_subscribing);

	boost::shared_ptr< serializer > & get_serializer();
	boost::shared_ptr< deserializer > & get_deserializer();

	void receive(const uint8_t *, uint32_t, const endpoint *, const endpoint *);

	service * find_multicast_service(ip_port _port) const;

private:
	void run_receiver();
	void run_sender();

	bool is_request(const message *_message) const;
	bool is_request(const uint8_t *, uint32_t) const;

	void do_send(client_id, std::vector< uint8_t > &);
	void do_broadcast(std::vector< uint8_t > &);
	void do_receive();

	void on_register_application(client_id, const std::string &, bool);
	void on_deregister_application(client_id);
	void on_provide_service(client_id, service_id, instance_id, const endpoint *);
	void on_withdraw_service(client_id, service_id, instance_id, const endpoint *);
	void on_start_service(client_id, service_id, instance_id);
	void on_stop_service(client_id, service_id, instance_id);
	void on_request_service(client_id, service_id, instance_id, const endpoint *);
	void on_release_service(client_id, service_id, instance_id, const endpoint *);
	void on_request_eventgroup(client_id, service_id, instance_id, eventgroup_id);
	void on_release_eventgroup(client_id, service_id, instance_id, eventgroup_id);

	void on_pong(client_id);

	void on_send_message(client_id, const uint8_t *, uint32_t);
	void on_update_field(uint8_t *, uint32_t);

	void on_register_method(client_id, service_id, instance_id, method_id);
	void on_deregister_method(client_id, service_id, instance_id, method_id);

	void send_ping(client_id);
	void send_application_info();
	void send_application_lost(const std::list< client_id > &);

	void send_request_service_ack(client_id, service_id, instance_id, const std::string &);
	void send_release_service_ack(client_id, service_id, instance_id);

	void process_command(std::size_t _bytes);
	void start_watchdog_cycle();
	void start_watchdog_check();

private:
	void open_cbk(boost::system::error_code const &_error, client_id _id);
	void create_cbk(boost::system::error_code const &_error);
	void destroy_cbk(boost::system::error_code const &_error);
	void send_cbk(boost::system::error_code const &_error, client_id _id);
	void receive_cbk(
			boost::system::error_code const &_error,
			std::size_t _bytes, unsigned int _priority);

	void watchdog_cycle_cbk(boost::system::error_code const &_error);
	void watchdog_check_cbk(boost::system::error_code const &_error);

	client_id find_local(service_id, instance_id, method_id) const;
	client * find_remote(service_id _service, instance_id _instance, bool _is_reliable) const;

	bool is_local(client_id) const;
	const endpoint * find_remote(client_id) const;

	void save_client_location(client_id, const endpoint *);

private:
	boost::shared_ptr< managing_proxy_impl > proxy_;

	boost::asio::io_service sender_service_;
	boost::asio::io_service::work sender_work_;
	boost::asio::io_service receiver_service_;
	boost::asio::io_service::work receiver_work_;
	boost::asio::io_service network_service_;
	boost::asio::io_service::work network_work_;

	boost::asio::system_timer watchdog_timer_;

	std::string queue_name_prefix_;
	std::string daemon_queue_name_;
	boost_ext::asio::message_queue daemon_queue_;

	uint8_t receive_buffer_[VSOMEIP_DEFAULT_QUEUE_SIZE];

	// buffers for sending messages
	std::deque< std::vector< uint8_t > > send_buffers_;

	// applications
	std::map< client_id, application_info > applications_;

	// requests (need to be stored as services may leave and come back)
	std::set< request_info > requests_;

	// Communication channels
	typedef std::map< client_id,
					  std::map< service_id,
					  	  	    std::map< instance_id,
					  	  	    	      std::set< method_id > > > > client_channel_map_t;
	typedef std::map< service_id,
			          std::map< instance_id,
			                    std::map< method_id,
			                    		  client_id > > > service_channel_map_t;

	client_channel_map_t client_channels_;
	service_channel_map_t service_channels_;

	// Client endpoints
	typedef std::map< client_id, const endpoint * > client_location_map_t;

	client_location_map_t client_locations_;

	// Events(?) & Fields
	struct eventgroup_info {
		std::set< client_id > clients_;
		std::set< event_id > events_;
	};
	std::map< service_id,
			  std::map< instance_id,
			  	  	    std::map< eventgroup_id,
			  	  	    		  eventgroup_info > > > eventgroups_;

	struct field_info {
		std::vector< uint8_t > payload_;
		std::set< client_id > _clients;
	};
	std::map< event_id, field_info > events_;

private:
	static client_id id__;
	sd::service_discovery *service_discovery_;

	std::string unicast_;

	boost::shared_ptr< serializer > serializer_;
	boost::shared_ptr< deserializer > deserializer_;

#ifdef VSOMEIP_DAEMON_DEBUG
private:
	void start_dump_cycle();
	void dump_cycle_cbk(boost::system::error_code const &);
	boost::asio::system_timer dump_timer_;
#endif
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_IMPL_HPP
