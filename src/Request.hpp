#pragma once

class Connection;

/*
	Represents a single request from a connection as it's being parsed.
*/
class Request
{
public:
	Request(Connection const &conn);

	/*
		Perform some work on the request.
		
		This may read incoming data, or write outgoing data, but should never block for an extended period of time.
	*/
	void workOnRequest();

	/*
		If true, workOnRequest should only be called if there is more incoming data available.
		If false, workOnRequest should be called whenever it's this Connection's turn to make progress.
	*/
	bool needsData();

	/*
		If true, this request has completed its work and can be torn down. Connection may parse more requests.
		If false, this request still needs more workOnRequest calls to complete.
	*/
	bool isDone();

private:
	Connection const &conn;
};
