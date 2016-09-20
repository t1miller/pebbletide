function locationError(err) {
  console.log('Error requesting location!');
}

function getTideData() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  var date = 'date=today&';
  var product = 'product=predictions&';
  var units = 'units=english&';  
  var station = 'station=9410170&';//San Diego
  var datum = 'datum=MLLW&';
  var time = 'time_zone=lst&';
  var interval = 'interval=h&';
  var format = 'format=json';
  var url = 'http://tidesandcurrents.noaa.gov/api/datagetter?'+ date + time + station + product + units + interval + datum + format;
  
  // Send request to NOAA TIDE
  xhrRequest(url, 'GET', 
    function(responseText) {      
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);    
      var cData = '';      
  
      //get all data and convert floats to ints by shifting decimal (pebble doesnt like floats)
      for (var i = 0; i < 24; i++) { 
        var temp = json.predictions[i].v;
        temp = temp.substring(0,3);//1.234 => 1.2
        temp = temp.replace('.','');//1.2 => 12
        cData += temp;
        console.log("Sending "+temp);
      }
          
      console.log(url);
             
      var dictionary = {"tideData":cData};
      
      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log('NOAA info sent to Pebble successfully!');
        },
        function(e) {
          console.log('Error sending NOAA info to Pebble!');
          console.log(e);
          //TODO lets display some local data or a pretty message!
        }
      );
    }
  );
}
// Get AppMessage events
Pebble.addEventListener('appmessage', function(e) {
  // Get the dictionary from the message
  var dict = e.payload;
  console.log('Got message: ' + JSON.stringify(dict));
  getTideData();
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
  getTideData();
});

Pebble.addEventListener('showConfiguration', function(e) {
  // Show config page
  Pebble.openURL('https://t1miller.github.io');
});

Pebble.addEventListener('webviewclosed', function(e) {
  // Decode and parse config data as JSON
  var configData = JSON.parse(decodeURIComponent(e.response));
  console.log('Config window returned: ', JSON.stringify(configData));

 if(configData.backgroundColor || configData.selectLocation){
   
   if(configData.selectLocation.localeCompare("San Diego")){
     configData.selectLocation = 0;
   }else if(configData.selectLocation.localeCompare("San Clemente")){
     configData.selectLocation = 1;
   }else if(configData.selectLocation.localeCompare("Santa Monica")){
     configData.selectLocation = 2;
   }
   
   Pebble.sendAppMessage({
     backgroundColor: parseInt(configData.backgroundColor,16),
     selectLocation: parseInt(configData.selectLocation,16)
   }, function() {
     console.log('Send succsesful');
   }, function(){
     console.log('Senf failed');
   });
 }
});
