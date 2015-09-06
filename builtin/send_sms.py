from twilio.rest import TwilioRestClient
 
# Your Account Sid and Auth Token from twilio.com/user/account
account_sid = "AC2323da24819dfe91a84a3a9c8632d770"
auth_token  = ""
client = TwilioRestClient(account_sid, auth_token)

def message():
	message = client.messages.create(body="Hi! Just wanted to let you know that I'm thinking about you. Love you!",
		to="+19177517628",
	  from_="+16464806816")
	return message.sid

print(message())

