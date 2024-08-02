var board;
var canvas;
var cam;
var assest = new Image();
var task_queue;

// camera control
var is_right_mouse_down = false;
var last_rclick_time = 0;
var last_rclick_position = [0, 0];
var panning = false;
var last_left_click = [0, 0];
var reveal_temporary = new Array();


// settings
const tile_size = 64;
const DISPLAY_ID = {
    "0": 0, 
    "1": 1, 
    "2": 2, 
    "3": 3, 
    "4": 4, 
    "5": 5, 
    "6": 6, 
    "7": 7, 
    "8": 8, 
    "bomb": 9,
    "unreveal": 10,
    "flagged": 11
}
const DIFFICULTY = {
    "easy":   .1, 
    "medium": .15,
    "hard":   .2
}
const SIZE = {
    "small":  [10, 10],
    "medium": [25, 25],
    "large":  [64, 64]
}

// game session
var grid = new Array()
var session_id = "";
var rows;
var cols;
var mine_density;
var bomb_remaining = 0;
var game_ended = false;
var hints = {
    "SAFE": [],
    "HIGH_PROBABILITY": [],
    "MINE": []
};

// map cell number to image coordinate
const img = [[0, 2], [0, 0], [1, 0], [2, 0], [3, 0], [0, 1], [1, 1], [2, 1], [3, 1]];
const img_size = 64;

const difficulty_buttons = document.querySelectorAll('#difficulty-container .select-button');
const size_buttons = document.querySelectorAll('#size-container .select-button');
const custom_size = document.getElementById('custom-size');

class Task_Queue {
    constructor() {
        this.queue = [];
        this.processing = false;
    }

    async add_task(task, data=null) {
        this.queue.push([task, data]);
        if (!this.processing) {
            this.processing = true;
            while (this.queue.length > 0) {
                const [curr_task, curr_data] = this.queue.shift();
                if (data === null){
                    await curr_task();
                } else {
                    await curr_task(curr_data);
                }
            }
            this.processing = false;
        }
    }
}

class Camera { 
    constructor(x, y, width, height) {
        this.pos = [x, y];
        this.height = height;
        this.width = width;
        this.zoom_ratio = 1;
        this.eff_width = width;
        this.eff_height = height;
        this.aspect_ratio  = width / height;
    }

    zoom(deltaY, [x, y]) {
        var oldZoomRatio = this.zoom_ratio;
        var zoom_ratioChanged = false;

        if (deltaY > 0) {
            // Zoom out
            this.zoom_ratio = Math.max(this.zoom_ratio / (deltaY / 300 + 1), Math.max(this.width / (cols * tile_size), this.height / (rows * tile_size)));
            zoom_ratioChanged = true;
        } else if (deltaY < 0) {
            // Zoom in
            this.zoom_ratio = Math.min(this.zoom_ratio * (-deltaY / 300 + 1), 1);
            zoom_ratioChanged = true;
        }
        
        // calculate new position for relatively zooming
        if (zoom_ratioChanged) {
            this.pos = [x - (x - this.pos[0]) * oldZoomRatio / this.zoom_ratio,
                      y - (y - this.pos[1]) * oldZoomRatio / this.zoom_ratio];
        }
        update();
    }

    move([x, y]) {
        this.pos[0] += x;
        this.pos[1] += y;
    }

    // Move camera relative to canvas movement
    move_relative([x, y]) {
        this.move([-x / board.width * this.eff_width, -y / board.height * this.eff_height]);
    }

    world_to_screen([x, y]) {
        return [(x - this.pos[0]) * board.width / this.eff_width,
                (y - this.pos[1]) * board.height / this.eff_height];
    }

    screen_to_world([x, y]) {
        return [x / board.width * this.eff_width + this.pos[0],
                y / board.height * this.eff_height + this.pos[1]];
    }

    screen_to_tile([x, y]) {
        return world_to_tile(this.screen_to_world([x, y]));
    }

    update() {
        this.width = this.height * board.width / board.height;
        this.eff_height = this.height / this.zoom_ratio;
        this.eff_width = this.width / this.zoom_ratio;

        // restrict camera movement
        this.pos[0] = Math.min(cols * tile_size - this.eff_width, this.pos[0]);
        this.pos[1] = Math.min(rows * tile_size - this.eff_height, this.pos[1]);
        this.pos[0] = Math.max(0, this.pos[0]);
        this.pos[1] = Math.max(0, this.pos[1]);
    }
}

window.onload = function() {
    board = document.getElementById("board");

    canvas = board.getContext("2d");
    canvas.imageSmoothingEnabled = false;


    init();
}


function new_game() {

    if (document.getElementById('custom').classList.contains('selected')) {
        cols = Number(document.getElementById('width').value);
        rows = Number(document.getElementById('height').value);
        if (cols === '' || cols <= 0 || !Number.isInteger(cols)) {
            alert("Invalid width value")
            return;
        } else if (rows == '' || rows <= 0 || !Number.isInteger(rows)) {
            alert("Invalid height value")
            return;
        }
        rows = Math.min(rows, 255);
        cols = Math.min(cols, 255);
    }

    end_current_session();
    clear_hints();
    game_ended = false;
    document.getElementById("new-game-button").src = "./public/assests/smile.png";

    for (var i = 0; i < cols; ++i) {
        grid[i] = new Array();
        for (var j = 0; j < rows; ++j) {
            grid[i][j] = DISPLAY_ID["unreveal"];
        }
    }

    // Get new session id 
    fetch("/new_session", {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json' 
        },
        body: JSON.stringify({rows, cols, mine_density})
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return response.json();  
    })
    .then(data => {
        session_id = data["session_id"];
        bomb_remaining = data["bomb_remaining"];
        update();
    })
    .catch(error => {
        console.error('Fetch error:', error);  
    });
}

async function right_click([x, y]) {
    if (game_ended)
        return;
    clear_hints();

    return fetch("/right_click", {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json' 
        },
        body: JSON.stringify({x, y, session_id})
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return response.json();  
    })
    .then(data => {
        for (var i = 0; i < data["updated_cell"].length; ++i) {
            grid[data["updated_cell"][i][0]][data["updated_cell"][i][1]] = data["display_id"][i]; 
        }
        bomb_remaining = data["bomb_remaining"];
        update();
    })
    .catch(error => {
        console.error('Fetch error:', error);  
    });
}

async function left_click([x, y]) {
    if (game_ended)
        return;

    clear_hints();
    return fetch("/left_click", {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json' 
        },
        body: JSON.stringify({x, y, session_id})
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return response.json();  
    })
    .then(data => {
        for (var i = 0; i < data["updated_cell"].length; ++i) {
            const x = data["updated_cell"][i][0];
            const y = data["updated_cell"][i][1];
            grid[x][y] = data["display_id"][i]; 
        }
        bomb_remaining = data["bomb_remaining"];
        if (data["game_status"] == "WIN") {
            win();
        } else if (data["game_status"] == "LOSE") {
            lose();
        }
        update();
    })
    .catch(error => {
        console.error('Fetch error:', error);  
    });
}

function win() {
    document.getElementById("new-game-button").src = "./public/assests/win.png";
    game_ended = true;
}

function lose() {
    document.getElementById("new-game-button").src = "./public/assests/game_over.png";
    game_ended = true;
}

function init() {

    task_queue = new Task_Queue();
    board.width = board.offsetWidth;
    board.height = board.offsetHeight;     
    assest.src = "./public/assests/minesweeper.bmp";

    mine_density = DIFFICULTY['easy'];
    rows = SIZE['medium'][0];
    cols = SIZE['medium'][1];
    document.getElementById('width').value = cols;
    document.getElementById('height').value = rows;

    cam = new Camera(0, 0, tile_size * 10, tile_size * 10 * board.offsetHeight / board.offsetWidth);

    assest.onload = function (){
        new_game();
        update();
    }

    board.addEventListener('contextmenu', (event) => {
        // prevent contextmenu
        event.preventDefault();
    });

    board.addEventListener('mousedown', (event) => {
        var [x, y] = cam.screen_to_tile([event.offsetX, event.offsetY]);

        if (event.button == 0 && is_valid([x, y]) && !game_ended) {
            if (grid[x][y] <= DISPLAY_ID["8"]) {
                for (var i = Math.max(0, x - 1 ); i <= Math.min(cols - 1, x + 1); ++i) {
                    for (var j = Math.max(0, y - 1); j <= Math.min(rows - 1, y + 1); ++j) {
                        if (grid[i][j] == DISPLAY_ID["unreveal"]) {
                            grid[i][j] = DISPLAY_ID["0"];
                            reveal_temporary.push([i,j]);
                        }
                    }
                }
            } else if (grid[x][y] == DISPLAY_ID["unreveal"]) {
                grid[x][y] = DISPLAY_ID["0"];
                reveal_temporary.push([x, y]);
            }
            last_left_click = [x, y];
        }

        if (event.button == 2) {
            last_rclick_time = new Date().getTime();
            last_rclick_position = cam.screen_to_world([event.offsetX, event.offsetY]);
            panning = false;
            is_right_mouse_down = true;
        }

        update()
    });

    window.addEventListener('mouseup', (event) => {
        var [x, y] = cam.screen_to_tile([event.offsetX, event.offsetY]);
        var timeTaken = new Date().getTime() - last_rclick_time;

        if (event.button == 2 && event.target.id == "board") {
            if (timeTaken < 90 || !is_panning([event.offsetX, event.offsetY])) {
                task_queue.add_task(right_click, [x, y]);
            }
        } 

        if (event.button == 0 ) {
            for (var i = 0; i < reveal_temporary.length; ++i) {
                if (grid[reveal_temporary[i][0]][reveal_temporary[i][1]] == DISPLAY_ID["0"])
                    grid[reveal_temporary[i][0]][reveal_temporary[i][1]] = DISPLAY_ID["unreveal"];
            }
            if (reveal_temporary.length && (last_left_click[0] != x || last_left_click[1] != y))
                update();
            reveal_temporary = new Array();
            if (event.target.id === "board" && last_left_click[0] == x && last_left_click[1] == y) {
                task_queue.add_task(left_click, [x, y]);
            }
        }
        is_right_mouse_down = false;
        panning = false;
    });

    window.addEventListener('mousemove', (event) => {
        if (is_right_mouse_down && is_panning([event.offsetX, event.offsetY])) {
            cam.move_relative([event.movementX, event.movementY]);
            update();
        }
    });

    board.addEventListener('wheel', (event) => {
        cam.zoom(event.deltaY, cam.screen_to_world([event.offsetX, event.offsetY]));
    });
    
}


// To differentiate right drag and right click
// [x, y] is current screen coordinate
function is_panning([x, y]) {
    var [a, b] = cam.screen_to_world([x, y]);
    var displacement = Math.sqrt((a - last_rclick_position[0])**2 + (b - last_rclick_position[1])**2);
    var timeTaken = new Date().getTime() - last_rclick_time;
    var velocity = displacement / timeTaken;

    // wait 20ms
    if (timeTaken < 20) {
        return false;
    }

    if (panning || velocity > .6 || displacement >= tile_size / 6) {
        return panning = true;
    }
    return false;
}

function window_resize() {
    board.width = board.offsetWidth;
    board.height = board.offsetHeight; 
    if (cam != undefined) {
        cam.update();
    }
}

function end_current_session() {
    if (session_id !== "") {
        const url = "/end_session";
        const data = JSON.stringify({ session_id });
        const blob = new Blob([data], { type: 'application/json' });
        
        if (!navigator.sendBeacon(url, blob)) {
            console.error('Failed to end session:', session_id);
        }
        session_id == "";
    }
}

function world_to_tile([x, y]) {
    return [Math.floor(x / tile_size), Math.floor(y/ tile_size)];
}

function is_valid([x, y]) {
    return x >= 0 && x < cols && y >= 0 && y <= rows;
}

function update_counter() {
    if (bomb_remaining > 999) {
        document.getElementById('digit1').src = './public/assests/9.png';
        document.getElementById('digit2').src = './public/assests/9.png';
        document.getElementById('digit3').src = './public/assests/9.png';
    } else if (bomb_remaining < -99) {
        document.getElementById('digit1').src = './public/assests/minus.png';
        document.getElementById('digit2').src = './public/assests/9.png';
        document.getElementById('digit3').src = './public/assests/9.png';
    } else if (bomb_remaining < 0){
        document.getElementById('digit1').src = './public/assests/minus.png';
        document.getElementById('digit2').src = './public/assests/' + Math.floor(-bomb_remaining / 10)  % 10 + '.png';
        document.getElementById('digit3').src = './public/assests/' + (-bomb_remaining) % 10 + '.png';
    } else {
        document.getElementById('digit1').src = './public/assests/' + Math.floor(bomb_remaining / 100) % 10 + '.png';
        document.getElementById('digit2').src = './public/assests/' + Math.floor(bomb_remaining / 10)  % 10 + '.png';
        document.getElementById('digit3').src = './public/assests/' + bomb_remaining % 10 + '.png';
    }
}

function clear_hints() {
    hints["SAFE"].length = 0;
    hints["HIGH_PROBABILITY"].length = 0;
    hints["MINE"].length = 0;
}

function update() {
    cam.update();

    update_counter();

    // clear canvas
    canvas.clearRect(0, 0, board.width, board.height);

    for (let x = Math.max(0, world_to_tile(cam.pos)[0]); x <= Math.min(world_to_tile([cam.pos[0] + cam.eff_width, cam.pos[1]])[0], cols - 1); ++x) {
        for (let y = Math.max(0, world_to_tile(cam.pos)[1]); y <= Math.min(world_to_tile([cam.pos[0], cam.pos[1] + cam.eff_height])[1], rows - 1); ++y) {
            var pos = cam.world_to_screen([x * tile_size, y * tile_size]);
            var dWidth = tile_size * board.width / cam.eff_width;
            var dHeight = tile_size * board.height / cam.eff_height;

            if (grid[x][y] <= DISPLAY_ID["8"]) {
                canvas.drawImage(assest, img[grid[x][y]][0] * img_size, img[grid[x][y]][1] * img_size, img_size, img_size, pos[0], pos[1], dWidth, dHeight);
            } else if (grid[x][y] === DISPLAY_ID["bomb"]) {
                canvas.drawImage(assest, 2 * img_size, 2 * img_size, img_size, img_size, pos[0], pos[1], dWidth, dHeight);
            } else if (grid[x][y] === DISPLAY_ID["unreveal"]) {
                canvas.drawImage(assest, 1 * img_size, 2 * img_size, img_size, img_size, pos[0], pos[1], dWidth, dHeight);
            } else if (grid[x][y] === DISPLAY_ID["flagged"]){
                canvas.drawImage(assest, 3 * img_size, 2 * img_size, img_size, img_size, pos[0], pos[1], dWidth, dHeight);
            } else {
                canvas.drawImage(assest, 1 * img_size, 2 * img_size, img_size, img_size, pos[0], pos[1], dWidth, dHeight);
                console.error("Invalid display id: ", [x, y, grid[x][y]]); 
            }
        }
    }

    canvas.fillStyle = 'rgba(0, 255, 0, 0.6)'

    for (let i = 0; i < hints["SAFE"].length; ++i ) {
        var pos = cam.world_to_screen([hints["SAFE"][i][0] * tile_size, hints["SAFE"][i][1] * tile_size]);
        var dWidth = tile_size * board.width / cam.eff_width;
        var dHeight = tile_size * board.height / cam.eff_height;
        canvas.fillRect(pos[0], pos[1], dWidth, dHeight);
    }

    canvas.fillStyle = 'rgba(255, 255, 0, 0.6)'

    for (let i = 0; i < hints["HIGH_PROBABILITY"].length; ++i ) {
        var pos = cam.world_to_screen([hints["HIGH_PROBABILITY"][i][0] * tile_size, hints["HIGH_PROBABILITY"][i][1] * tile_size]);
        var dWidth = tile_size * board.width / cam.eff_width;
        var dHeight = tile_size * board.height / cam.eff_height;
        canvas.fillRect(pos[0], pos[1], dWidth, dHeight);
    }

    canvas.fillStyle = 'rgba(255, 0, 0, 0.6)'

    for (let i = 0; i < hints["MINE"].length; ++i ) {
        var pos = cam.world_to_screen([hints["MINE"][i][0] * tile_size, hints["MINE"][i][1] * tile_size]);
        var dWidth = tile_size * board.width / cam.eff_width;
        var dHeight = tile_size * board.height / cam.eff_height;
        canvas.fillRect(pos[0], pos[1], dWidth, dHeight);
    }
}


async function get_hint(){
    if (game_ended)
        return;

    clear_hints();
    return fetch("/get_hint", {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json' 
        },
        body: JSON.stringify({session_id})
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return response.json();  
    })
    .then(json => {
        for (var i = 0; i < json["hints"].length; ++i) {
            if (json["hints"][i]["HINT_TYPE"] == "SAFE"){
                hints["SAFE"].push([json["hints"][i]["x"], json["hints"][i]["y"]]);
            } else if (json["hints"][i]["HINT_TYPE"] == "HIGH_PROBABILITY"){
                hints["HIGH_PROBABILITY"].push([json["hints"][i]["x"], json["hints"][i]["y"]]);
            } else {
                hints["MINE"].push([json["hints"][i]["x"], json["hints"][i]["y"]]);
            }
        }
        update();
    })
    .catch(error => {
        console.error('Fetch error:', error);  
    });
}

difficulty_buttons.forEach(button => {
    button.addEventListener('click', () => {
        difficulty_buttons.forEach(btn => btn.classList.remove('selected'));
        button.classList.add('selected');
        mine_density = DIFFICULTY[button.id]
    });
});

size_buttons.forEach(button => {
    button.addEventListener('click', () => {
        size_buttons.forEach(btn => btn.classList.remove('selected'));
        button.classList.add('selected');
        if (button.id === 'custom') {
            custom_size.style.display = 'flex';
        } else {
            custom_size.style.display = 'none';
            rows = SIZE[button.id][0];
            cols = SIZE[button.id][1];
            document.getElementById('width').value = cols;
            document.getElementById('height').value = rows;
        } 
    });
});

window.addEventListener('beforeunload', end_current_session);
window.ondragstart = function() {return false;}
window.addEventListener('resize', () => {window_resize(); update();});
document.getElementById("hint-button").onclick = function (){task_queue.add_task(get_hint);};
document.getElementById("new-game-button").onclick = new_game;
