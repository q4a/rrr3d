#include "stdafx.h"

#include "net/NetConnectionTCP.h"
#include "net/NetService.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{

unsigned ReserveLoopbackPort(int socketType)
{
	const int descriptor = socket(AF_INET, socketType, 0);
	if (descriptor < 0)
		return 0;

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = 0;
	if (bind(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		close(descriptor);
		return 0;
	}

	socklen_t size = sizeof(address);
	if (getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &size) != 0)
	{
		close(descriptor);
		return 0;
	}

	const unsigned port = ntohs(address.sin_port);
	close(descriptor);
	return port;
}

unsigned ReserveServicePort()
{
	for (unsigned attempt = 0; attempt < 32; ++attempt)
	{
		const int tcpDescriptor = socket(AF_INET, SOCK_STREAM, 0);
		if (tcpDescriptor < 0)
			return 0;

		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		if (bind(tcpDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
		{
			close(tcpDescriptor);
			continue;
		}

		socklen_t size = sizeof(address);
		if (getsockname(tcpDescriptor, reinterpret_cast<sockaddr*>(&address), &size) != 0)
		{
			close(tcpDescriptor);
			continue;
		}

		const int udpDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
		const bool udpAvailable = udpDescriptor >= 0 &&
			bind(udpDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
		const unsigned port = udpAvailable ? ntohs(address.sin_port) : 0;
		if (udpDescriptor >= 0)
			close(udpDescriptor);
		close(tcpDescriptor);
		if (port != 0)
			return port;
	}
	return 0;
}

bool TestBitStreamOwnershipAndWindowsLongLayout()
{
	net::NetCmdHeader cmdHeader;
	cmdHeader.id = 0x1ff;
	cmdHeader.target = 0x1f;
	cmdHeader.rpc = 0x1f;
	cmdHeader.size = 0xfff;
	unsigned cmdBits = 0;
	std::memcpy(&cmdBits, &cmdHeader, sizeof(cmdBits));

	net::NetStateHeader stateHeader;
	stateHeader.time = 0x7fffffffu;
	stateHeader.id = 0x1ff;
	stateHeader.sender = 7;
	stateHeader.size = 0x1ff;
	unsigned stateBits[2] = {};
	std::memcpy(stateBits, &stateHeader, sizeof(stateBits));
	if (cmdBits != 0xfffffffeu || stateBits[0] != 0x7fffffffu ||
		stateBits[1] != 0x001fffffu)
	{
		std::cerr << "Network bitfield wire layout differs from the Windows protocol\n";
		return false;
	}

	const unsigned marker = 0x4d525252u;
	void* source = const_cast<unsigned*>(&marker);
	long sourceLong = 0x12345678L;

	net::BitStream writer;
	writer.Reset(true, false, 1);
	writer.Serialize(source, sizeof(marker), false);
	writer.Serialize(sourceLong);

	boost::asio::streambuf packet;
	std::ostream output(&packet);
	writer.Write(output, false, false, false);

	net::BitStream reader;
	reader.Reset(false, true, 1);
	std::istream input(&packet);
	reader.Read(input);

	void* decoded = NULL;
	long decodedLong = 0;
	reader.Serialize(decoded, sizeof(marker), false);
	reader.Serialize(decodedLong);

	const bool passed = decoded != NULL &&
		std::memcmp(decoded, &marker, sizeof(marker)) == 0 &&
		decodedLong == sourceLong &&
		net::BitValue::GetSize(net::btLong) == 4;
	if (!passed)
	{
		unsigned decodedMarker = 0;
		if (decoded != NULL)
			std::memcpy(&decodedMarker, decoded, sizeof(decodedMarker));
		std::cerr << "BitStream detail: marker=" << std::hex << decodedMarker
			<< " long=" << decodedLong << " expected=" << sourceLong
			<< std::dec << " wireLongSize=" << net::BitValue::GetSize(net::btLong)
			<< '\n';
	}
	free(decoded);
	return passed;
}

bool TestUdpLoopback()
{
	const unsigned port = ReserveLoopbackPort(SOCK_DGRAM);
	if (port == 0)
		return false;

	boost::asio::io_service ioService;
	net::NetAcceptorTCP acceptor(ioService);
	net::NetChannelTCP receiver(&acceptor);
	net::NetChannelTCP sender(&acceptor);
	receiver.Open(false);
	receiver.Bind(net::Endpoint(0, port));
	sender.Open(false);

	const char payload[] = "RRR3D UDP state";
	if (!sender.Send(net::Endpoint("127.0.0.1", port), payload, sizeof(payload)))
		return false;

	char received[sizeof(payload)] = {};
	for (unsigned attempt = 0; attempt < 500; ++attempt)
	{
		ioService.restart();
		ioService.poll();

		unsigned available = 0;
		if (receiver.IsAvailable(available, false) && available >= sizeof(payload))
		{
			unsigned count = 0;
			net::Endpoint remote;
			const bool ok = receiver.Receive(received, sizeof(received), count, remote, false);
			return ok && count == sizeof(payload) &&
				std::memcmp(received, payload, sizeof(payload)) == 0;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return false;
}

struct ServiceUser: net::INetServiceUser
{
	unsigned connections = 0;
	unsigned callbacks = 0;
	unsigned receivedCommands = 0;
	unsigned payload = 0;
	unsigned lastId = 0;
	unsigned lastRpc = 0;
	unsigned lastSize = 0;
	unsigned failureError = 0;
	unsigned disconnects = 0;

	bool OnConnected(net::INetConnection*) override
	{
		++connections;
		return true;
	}

	void OnConnectionFailed(net::INetConnection*, unsigned error) override
	{
		failureError = error;
	}

	void OnDisconnected(net::INetConnection*) override
	{
		++disconnects;
	}

	void OnReceiveCmd(const net::NetMessage&, const net::NetCmdHeader& header,
		const void* data, unsigned size) override
	{
		++callbacks;
		lastId = header.id;
		lastRpc = header.rpc;
		lastSize = size;
		if (header.id == net::INetPlayer::cDefCmd && header.rpc == 7 &&
			size == sizeof(payload))
		{
			std::memcpy(&payload, data, size);
			++receivedCommands;
		}
	}
};

bool Pump(net::NetService& server, net::NetService& client,
	const std::function<bool()>& complete, unsigned& clock,
	unsigned timeoutMilliseconds)
{
	for (unsigned elapsed = 0; elapsed < timeoutMilliseconds; ++elapsed)
	{
		++clock;
		server.Process(clock);
		client.Process(clock);
		if (complete())
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return false;
}

bool TestTcpServiceLoopback()
{
	const unsigned port = ReserveServicePort();
	if (port == 0)
	{
		std::cerr << "TCP detail: could not reserve a port\n";
		return false;
	}

	ServiceUser serverUser;
	ServiceUser clientUser;
	net::NetService server;
	net::NetService client;
	server.user(&serverUser);
	client.user(&clientUser);
	server.Initializate();
	client.Initializate();
	lsl::StringVec adapterAddresses;
	if (!server.GetAdapterAddresses(adapterAddresses))
	{
		std::cerr << "TCP detail: adapter enumeration failed\n";
		return false;
	}
	server.StartServer(port, NULL);
	if (!client.Connect(net::Endpoint("127.0.0.1", port), NULL))
	{
		std::cerr << "TCP detail: Connect rejected endpoint\n";
		return false;
	}

	unsigned clock = 0;
	if (!Pump(server, client, [&]() { return client.isConnected(); }, clock, 2000))
	{
		std::cerr << "TCP detail: player-id handshake timed out; serverConnections="
			<< server.connectionCount() << " serverCallbacks=" << serverUser.connections
			<< " clientCallbacks=" << clientUser.connections << '\n';
		return false;
	}
	if (server.connectionCount() != 1 || serverUser.connections != 1 ||
		clientUser.connections != 1)
	{
		std::cerr << "TCP detail: handshake counters differ; serverConnections="
			<< server.connectionCount() << " serverCallbacks=" << serverUser.connections
			<< " clientCallbacks=" << clientUser.connections << '\n';
		return false;
	}

	const unsigned payload = 0x51a7c0deu;
	std::ostream& command = client.player()->NewCmd(net::INetPlayer::cDefCmd,
		net::cServerPlayer, 7);
	net::Write(command, payload);
	client.player()->CloseCmd();

	if (!Pump(server, client, [&]() { return serverUser.receivedCommands == 1; }, clock, 2000))
	{
		net::INetConnection* clientConnection = client.GetConnection(0);
		net::INetConnection* serverConnection = server.GetConnection(0);
		std::cerr << "TCP detail: command timed out; count="
			<< serverUser.receivedCommands
			<< " callbacks=" << serverUser.callbacks
			<< " lastId=" << serverUser.lastId
			<< " lastRpc=" << serverUser.lastRpc
			<< " lastSize=" << serverUser.lastSize
			<< " clientSent=" << (clientConnection ? clientConnection->bytesSend() : 0)
			<< " serverReceived=" << (serverConnection ? serverConnection->bytesReceived() : 0)
			<< " clientOpen=" << (clientConnection != NULL)
			<< '\n';
		return false;
	}
	if (serverUser.payload != payload)
	{
		std::cerr << "TCP detail: payload mismatch\n";
		return false;
	}

	client.Close();
	if (client.isConnected() || client.isConnecting())
	{
		std::cerr << "TCP detail: client state was not reset on Close\n";
		return false;
	}
	if (!client.Connect(net::Endpoint("127.0.0.1", port), NULL))
	{
		std::cerr << "TCP detail: reconnect rejected endpoint\n";
		return false;
	}
	if (!Pump(server, client, [&]() { return client.isConnected(); }, clock, 3000))
	{
		std::cerr << "TCP detail: reconnect handshake timed out; serverConnections="
			<< server.connectionCount() << '\n';
		return false;
	}
	if (!Pump(server, client, [&]() { return server.connectionCount() == 1; }, clock, 2000))
	{
		std::cerr << "TCP detail: disconnected server peer was not released\n";
		return false;
	}
	server.Close();
	if (!Pump(server, client, [&]() { return client.IsClosed(); }, clock, 2000) ||
		clientUser.disconnects == 0)
	{
		std::cerr << "TCP detail: remote shutdown did not close the client cleanly\n";
		return false;
	}

	client.Finalizate();
	server.Finalizate();

	const unsigned closedPort = ReserveLoopbackPort(SOCK_STREAM);
	ServiceUser failedUser;
	net::NetService failedClient;
	failedClient.user(&failedUser);
	failedClient.Initializate();
	if (closedPort == 0 ||
		!failedClient.Connect(net::Endpoint("127.0.0.1", closedPort), NULL))
		return false;
	for (unsigned time = 1; time <= 2000 && failedUser.failureError == 0; ++time)
	{
		failedClient.Process(time);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (failedUser.failureError == 0 || !failedClient.IsClosed())
	{
		std::cerr << "TCP detail: refused connection did not report/close cleanly\n";
		return false;
	}
	failedClient.Finalizate();
	return true;
}

}

int main()
{
	if (!TestBitStreamOwnershipAndWindowsLongLayout())
	{
		std::cerr << "BitStream ownership/wire-layout regression failed\n";
		return 1;
	}
	if (!TestUdpLoopback())
	{
		std::cerr << "UDP loopback regression failed\n";
		return 2;
	}
	if (!TestTcpServiceLoopback())
	{
		std::cerr << "TCP service loopback regression failed\n";
		return 3;
	}

	std::cout << "NetLib TCP/UDP loopback smoke passed\n";
	return 0;
}
