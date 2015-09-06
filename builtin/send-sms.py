from twilio.rest import TwilioRestClient

account_sid = "AC2323da24819dfe91a84a3a9c8632d770"
auth_token  = "3b499cb1aeed9cc7dd392295f661910d"
client = TwilioRestClient(account_sid, auth_token)

message = client.messages.create(body="Hi! Just wanted to let you know that I'm thinking about you. Love you!",
	to="Your num",
	from_="+16464806816")
print message.sid



