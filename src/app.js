Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
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
