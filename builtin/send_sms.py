from twilio.rest import TwilioRestClient
 
# Your Account Sid and Auth Token from twilio.com/user/account
account_sid = "ACfc927d066ae91adf7dd19dbbe9b9ba46"
auth_token  = ""
client = TwilioRestClient(account_sid, auth_token)
 
message = client.messages.create(body="Jenny please?! I love you <3",
    to="+13472608289",    # Replace with your phone number
    from_="") # Replace with your Twilio number
print message.sid
