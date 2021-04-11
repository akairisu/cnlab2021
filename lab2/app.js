const express = require("express");
const bodyParser = require("body-parser");
const {spawn} = require('child_process');
const {exec} = require('child_process');
let app = express();
app.use(bodyParser.urlencoded({extended: true}));

app.get(/\/*/, (req, res) => {
    let ip = req.headers['x-forwarded-for'] || req.connection.remoteAddress;
    let remote_ip = ip.split(":")[3];
    console.log(`${remote_ip} is asking for wifi!`);
    res.setHeader("Context-type", "text/html")
    res.send(`
        <html>
            <form action="login" method="post">
                name: <input type="text" name="name" />
                </br>
                password: <input type="password" name="password" />
                </br>
                <button>GO!</button>
            </form>
        </html>`
        );
});

app.post("/login", (req, res) => {
    console.log(req.body)
    let name = req.body.name;
    let password = req.body.password;
    let ip = req.headers['x-forwarded-for'] || req.connection.remoteAddress;
    let remote_ip = ip.split(":")[3];
    console.log(remote_ip)
    if(name == "kirito" && password == "starburst") {
        res.send("<h1>Loading success </h1>");
        //TODO
        spawn("iptables", ["-t", "nat", "-I", "PREROUTING", "1", "-s", remote_ip, "-j", "ACCEPT"])
        spawn("iptables", ["-t", "nat", "-I", "PREROUTING", "1", "-d", remote_ip, "-j", "ACCEPT"])
        spawn("iptables", ["-I", "FORWARD", "-s", remote_ip, "-j", "ACCEPT"])
        spawn("iptables", ["-I", "FORWARD", "-d", remote_ip, "-j", "ACCEPT"])
		spawn("iptables", ["-L", "-v", "-x"])
        
    } else {
        res.send("<h1>Error</h1>")
    }
});

app.listen(9090);
console.log("Start listening!");

exec("iptables -L -v -x", (error, stdout, stderr) =>{
	let lines = stdout.split('\n');
	let flag = 0;
	let data = [];
	for(let i = 0; i < lines.length; i++){
		let fields = lines[i].trim().split(/\s+/, 9);
		console.log(lines[i]);
		if(fields[0] === 'Chain' && fields[1] === 'FORWARD'){
			console.log(fields[0]);
			flag = 1;
			continue;
		}
		if(flag === 1 && fields[0] !== 'pkts'){
			data.push({pkts: fields[0], bytes: fields[1], src: fields[7], dest: fields[8]});
		}
		if(flag === 1 && fields[0] === ''){
			break;
		}
	}
	for(let i = 0; i < data.length; i++){
		console.log(data[i]['pkts']);
	}
});
