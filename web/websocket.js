console.log('opening websocket');
const socket = new WebSocket("ws://localhost:8080");
socket.onopen = (event) => {
  console.log("websocket open");
};
socket.addEventListener("message", (event) => {
  console.log("Message from server");
  window.data = JSON.parse(event.data).eeg_buffer;
});
