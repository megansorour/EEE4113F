// let monitoringInterval;

// function startMonitoring() {
//     monitoringInterval = setInterval(updateWeight, 1000); // Update weight every second
//     document.getElementById('startButton').disabled = true;
//     document.getElementById('stopButton').disabled = false;
// }

// function stopMonitoring() {
//     clearInterval(monitoringInterval);
//     document.getElementById('startButton').disabled = false;
//     document.getElementById('stopButton').disabled = true;
// }

// function updateWeight() {
//     // Replace this with your actual weight monitoring logic
//     let weight = Math.floor(Math.random() * 1000); // Random weight for demonstration
//     document.getElementById('weightDisplay').innerText = `Weight: ${weight} grams`;
// }

let monitoringInterval;

function startMonitoring() {
    monitoringInterval = setInterval(update_weight, 1000); // Update weight every second
    document.querySelector('button[type="submit"]').disabled = true;
}

function stopMonitoring() {
    clearInterval(monitoringInterval);
    document.querySelector('button[type="submit"]').disabled = false;
}

function update_weight() {
    // Replace this with your actual weight monitoring logic
    let weight = Math.floor(Math.random() * 1000); // Random weight for demonstration
    document.querySelector('p').innerText = `Weight: ${weight} grams`;
}
