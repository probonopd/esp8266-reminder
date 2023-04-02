// Based on https://github.com/SensorsIot/Reminder-with-Google-Calender/ by Andreas Spiess
// DO NOT FORGET TO DEPLOY A NEW VERSION AFTER MAKING CHANGES

/*
Events for the next hour
https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec?action=listEvents

{"events":[{"title":"MyEventNow","description":"https://foo.bar","minutesFromNow":-30}]}

Confirm an event
https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec?action=confirmEvent&eventTitle=MyEventNow

*/

function doGet(request) {
  var action = request.parameter['action'];
  var eventTitle = request.parameter['eventTitle'];
  
  if (action == 'listEvents') {
    var events = GetEvents();
    var response = {
      'events': events
    };
    return ContentService.createTextOutput(JSON.stringify(response))
      .setMimeType(ContentService.MimeType.JSON);
  } else if (action == 'confirmEvent' && eventTitle) {
    var confirmationMessage = ConfirmEvent(eventTitle);
    return ContentService.createTextOutput(confirmationMessage);
  } else {
    return ContentService.createTextOutput('Invalid action or missing event title parameter.');
  }
}


function doPost(e) {
  var eventTitle = e.parameter.title;
  if (eventTitle) {
    ConfirmEvent(eventTitle);
  }
}

function ConfirmEvent(eventTitle) {
  var _calendarName = 'ESP32Tasks';
  var Cal = CalendarApp.getCalendarsByName(_calendarName)[0];
  var Now = new Date();
  var Later = new Date();
  Later.setHours(Later.getHours() + 1);
  var events = Cal.getEvents(Now, Later);
  var event = Cal.getEvents(new Date(), Later, {search: eventTitle})[0];
  if (event) {
    var doneEventTitle = eventTitle + ' done';
    var doneEvent = Cal.getEvents(event.getStartTime(), event.getEndTime(), {search: doneEventTitle})[0];
    if (!doneEvent) {
      var doneEventStart = event.getStartTime();
      var doneEventEnd = event.getEndTime();
      var doneEventDescription = 'Completed ' + eventTitle + ' at ' + new Date().toLocaleTimeString();
      Cal.createEvent(doneEventTitle, doneEventStart, doneEventEnd, {description: doneEventDescription});
      return 'Event confirmed: ' + eventTitle;
    } else {
      return 'Event already confirmed: ' + eventTitle;
    }
  } else {
    return 'Event not found: ' + eventTitle;
  }
}

function GetEvents() {
  var _calendarName = 'ESP32Tasks';
  var Cal = CalendarApp.getCalendarsByName(_calendarName)[0];
  var Now = new Date();
  var Later = new Date();
  Later.setHours(Later.getHours() + 1);
  var events = Cal.getEvents(Now, Later);
  var arr = [];
  for (var i = 0; i < events.length; i++) {
    var event = events[i];
    var title = event.getTitle();
    var description = event.getDescription();
    var doneEventTitle = title + ' done';
    var doneEvent = Cal.getEvents(event.getStartTime(), event.getEndTime(), {search: doneEventTitle});
    if (doneEvent.length == 0) {
      var minutesFromNow = Math.round((event.getStartTime().getTime() - Now.getTime()) / (1000 * 60));
      arr.push({
        'title': title,
        'description': description,
        'minutesFromNow': minutesFromNow
      });
    }
  }
  return arr;
}
