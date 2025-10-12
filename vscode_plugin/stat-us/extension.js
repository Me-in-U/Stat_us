const vscode = require("vscode");
const http = require("http");
const https = require("https");

// Internal state
let intervalHandle = null;
let lastActivityAt = Date.now();
let statusBarItem;
let isRunning = false;
let sessionStartedAt = Date.now();
let gContext = null;
let lastSampleAt = Date.now();
let totalActiveMs = 0;
let keystrokes = 0;

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
	console.log("Stat-us extension activated");
	gContext = context;
	sessionStartedAt = Date.now();

	// Status bar
	statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
	statusBarItem.text = "$(cloud-upload) Stat-us: idle";
	statusBarItem.tooltip = "Stat-us 전송 상태";
	statusBarItem.command = "stat-us.sendNow";
	statusBarItem.show();
	context.subscriptions.push(statusBarItem);

	// Commands
	context.subscriptions.push(
		vscode.commands.registerCommand("stat-us.start", startSending),
		vscode.commands.registerCommand("stat-us.stop", stopSending),
		vscode.commands.registerCommand("stat-us.sendNow", async () => {
			const cfg = getConfig();
			if (!cfg.backendUrl) {
				vscode.window.showWarningMessage("Stat-us: backendUrl 설정이 필요합니다.");
				return;
			}
			const payload = await buildPayload();
			await sendPayload(cfg, payload, true);
		}),
		vscode.commands.registerCommand("stat-us.setApiKey", async () => {
			const apiKey = await vscode.window.showInputBox({
				prompt: "API 키를 입력하세요",
				ignoreFocusOut: true,
				password: true,
			});
			if (typeof apiKey === "string") {
				await vscode.workspace.getConfiguration("stat-us").update("apiKey", apiKey, vscode.ConfigurationTarget.Global);
				vscode.window.showInformationMessage("Stat-us: API 키가 설정되었습니다.");
			}
			const backendUrl = await vscode.window.showInputBox({
				prompt: "백엔드 수신 URL을 입력하세요 (예: http://localhost:8080/api/ingest/vscode)",
				ignoreFocusOut: true,
				value: vscode.workspace.getConfiguration("stat-us").get("backendUrl") || "",
			});
			if (typeof backendUrl === "string") {
				await vscode.workspace.getConfiguration("stat-us").update("backendUrl", backendUrl, vscode.ConfigurationTarget.Global);
				vscode.window.showInformationMessage("Stat-us: 백엔드 URL이 설정되었습니다.");
			}
		})
	);

	// Activity listeners to track idle time
	// Activity listeners to track idle time and keystrokes
	context.subscriptions.push(
		vscode.window.onDidChangeActiveTextEditor(() => {
			lastActivityAt = Date.now();
		}),
		vscode.window.onDidChangeTextEditorSelection(() => {
			lastActivityAt = Date.now();
		}),
		vscode.workspace.onDidChangeTextDocument((e) => {
			lastActivityAt = Date.now();
			if (e?.contentChanges?.length) {
				keystrokes += e.contentChanges.length;
			}
		})
	);

	// Auto-start based on configuration
	const cfg = getConfig();
	if (cfg.enable) {
		startSending();
	}

	// React to configuration changes
	context.subscriptions.push(
		vscode.workspace.onDidChangeConfiguration((e) => {
			if (!e.affectsConfiguration("stat-us")) return;
			const newCfg = getConfig();
			if (newCfg.enable && !isRunning) {
				startSending();
			} else if (!newCfg.enable && isRunning) {
				stopSending();
			} else if (isRunning && (e.affectsConfiguration("stat-us.intervalSeconds") || e.affectsConfiguration("stat-us.idleThresholdSeconds"))) {
				// restart timer to apply new interval/idle settings
				stopSending();
				startSending();
			}
		})
	);
}

function deactivate() {
	stopSending();
}

function getConfig() {
	const c = vscode.workspace.getConfiguration("stat-us");
	return {
		enable: c.get("enable"),
		backendUrl: c.get("backendUrl"),
		apiKey: c.get("apiKey"),
		intervalSeconds: c.get("intervalSeconds"),
		idleThresholdSeconds: c.get("idleThresholdSeconds"),
		sendCode: c.get("sendCode"),
		maxCodeLength: c.get("maxCodeLength"),
	};
}

async function startSending() {
	if (isRunning) return;
	const cfg = getConfig();
	if (!cfg.backendUrl) {
		vscode.window.showWarningMessage("Stat-us: backendUrl 설정이 필요합니다.");
		return;
	}
	isRunning = true;
	updateStatusBar("running");

	// Initial immediate send
	try {
		const payload = await buildPayload();
		await sendPayload(cfg, payload, true);
	} catch (e) {
		console.warn("Stat-us: initial send failed", e);
	}

	// Set interval
	intervalHandle = setInterval(async () => {
		const cfgLoop = getConfig();
		try {
			const payload = await buildPayload();
			await sendPayload(cfgLoop, payload, false);
		} catch (e) {
			console.warn("Stat-us: periodic send failed", e);
		}
	}, Math.max(5, Number(cfg.intervalSeconds || 60)) * 1000);
}

function stopSending() {
	if (intervalHandle) {
		clearInterval(intervalHandle);
		intervalHandle = null;
	}
	isRunning = false;
	updateStatusBar("stopped");
}

function updateStatusBar(state, extra) {
	if (!statusBarItem) return;
	let icon = "$(cloud-upload)";
	let text;
	const suffixParen = extra ? ` (${extra})` : "";
	const suffixSpace = extra ? ` ${extra}` : "";
	switch (state) {
		case "running":
			text = "Stat-us: on" + suffixParen;
			break;
		case "stopped":
			text = "Stat-us: off";
			break;
		case "sending":
			icon = "$(sync)";
			text = "Stat-us: sending" + suffixSpace;
			break;
		case "error":
			icon = "$(error)";
			text = "Stat-us: error" + suffixSpace;
			break;
		default:
			text = "Stat-us";
	}
	statusBarItem.text = `${icon} ${text}`;
}

async function buildPayload() {
	const cfg = getConfig();
	const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;

	const { filePath, languageId, code } = readEditorInfo(cfg);
	const branch = await readGitBranchSafe();

	// Idle status and active time accounting
	const now = Date.now();
	const idleForMs = now - lastActivityAt;
	const isIdle = idleForMs >= Math.max(10, Number(cfg.idleThresholdSeconds || 60)) * 1000;
	const elapsed = now - lastSampleAt;
	if (elapsed > 0) {
		if (!isIdle) totalActiveMs += elapsed;
		lastSampleAt = now;
	}

	const payload = {
		timestamp: new Date().toISOString(),
		workspaceRoot,
		filePath,
		languageId,
		branch,
		isIdle,
		idleForMs,
		sessionMs: Date.now() - sessionStartedAt,
		sessionActiveMs: totalActiveMs,
		keystrokes,
		vscodeVersion: vscode.version,
		extensionVersion: gContext?.extension?.packageJSON?.version,
	};

	if (cfg.sendCode && code) {
		payload.code = code;
		payload.codeLength = code.length;
	}

	return payload;
}

function readEditorInfo(cfg) {
	const editor = vscode.window.activeTextEditor;
	if (!editor?.document) return { filePath: undefined, languageId: undefined, code: undefined };
	const filePath = editor.document.uri.fsPath;
	const languageId = editor.document.languageId;
	let code;
	if (cfg.sendCode === true && Number(cfg.maxCodeLength) > 0) {
		const content = editor.document.getText();
		if (content && content.length > 0) {
			code = content.slice(0, Number(cfg.maxCodeLength));
		}
	}
	return { filePath, languageId, code };
}

async function readGitBranchSafe() {
	try {
		const gitExt = vscode.extensions.getExtension("vscode.git");
		if (!gitExt) return undefined;
		const git = gitExt.isActive ? gitExt.exports : await gitExt.activate();
		const api = git?.getAPI?.(1);
		const repo = api?.repositories?.[0];
		return repo?.state?.HEAD?.name;
	} catch (err) {
		console.warn("Stat-us: failed to read git branch", err);
		return undefined;
	}
}

async function sendPayload(cfg, payload, showToastOnSuccess = false) {
	if (!cfg.backendUrl) return;
	updateStatusBar("sending");
	try {
		const headers = { "Content-Type": "application/json" };
		if (cfg.apiKey) headers["x-api-key"] = cfg.apiKey;
		const res = await postJson(cfg.backendUrl, payload, headers, 8000);
		updateStatusBar("running", `ok ${new Date().toLocaleTimeString()}`);
		if (showToastOnSuccess) {
			vscode.window.showInformationMessage(`Stat-us 전송 성공 (${res.status})`);
		}
	} catch (err) {
		updateStatusBar("error");
		const msg = err?.response ? `${err.response.status} ${err.response.statusText}` : err?.message || String(err);
		console.error("Stat-us send error:", msg);
		vscode.window.showWarningMessage(`Stat-us 전송 실패: ${msg}`);
		throw err;
	}
}

function postJson(urlStr, data, headers = {}, timeoutMs = 8000) {
	return new Promise((resolve, reject) => {
		try {
			const url = new URL(urlStr);
			const isHttps = url.protocol === "https:";
			const lib = isHttps ? https : http;
			const body = Buffer.from(JSON.stringify(data));
			const options = {
				method: "POST",
				hostname: url.hostname,
				port: url.port || (isHttps ? 443 : 80),
				path: url.pathname + url.search,
				headers: {
					"Content-Type": "application/json",
					"Content-Length": body.length,
					...headers,
				},
			};
			const req = lib.request(options, (res) => {
				const chunks = [];
				res.on("data", (d) => chunks.push(d));
				res.on("end", () => {
					const respBody = Buffer.concat(chunks).toString("utf8");
					if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
						resolve({ status: res.statusCode, body: respBody });
					} else {
						const err = new Error(`HTTP ${res.statusCode}: ${respBody}`);
						// @ts-ignore add response for consistent error message above
						err.response = { status: res.statusCode, statusText: res.statusMessage };
						reject(err);
					}
				});
			});
			req.on("error", reject);
			req.setTimeout(timeoutMs, () => {
				req.destroy(new Error("Request timeout"));
			});
			req.write(body);
			req.end();
		} catch (e) {
			reject(e);
		}
	});
}

module.exports = {
	activate,
	deactivate,
};
