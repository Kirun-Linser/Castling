package main

import (
	"os/exec"
	"strings"
)

func main() {
	before := memLine()
	// 提权执行 purge（macOS 弹出系统密码框）
	err := exec.Command("osascript", "-e",
		`do shell script "purge" with administrator privileges`).Run()
	if err != nil {
		dialog("清理已取消或失败。", "caution")
		return
	}
	after := memLine()
	msg := "内存清理完成！\n\n清理前：" + before + "\n清理后：" + after +
		"\n\n非活跃内存与系统缓存已清除。"
	dialog(msg, "note")
}

func memLine() string {
	out, _ := exec.Command("bash", "-c", "memory_pressure | grep 'System-wide memory free' | tail -1").CombinedOutput()
	line := strings.TrimSpace(string(out))
	if line != "" {
		// "System-wide memory free percentage: 67%" -> "内存可用 67%"
		if i := strings.Index(line, ":"); i >= 0 {
			return "内存可用 " + strings.TrimSpace(line[i+1:])
		}
		return line
	}
	// 备用：vm_stat
	out2, _ := exec.Command("bash", "-c", "vm_stat | head -4").CombinedOutput()
	return strings.TrimSpace(string(out2))
}

func dialog(msg string, icon string) {
	esc := strings.NewReplacer("\\", "\\\\", "\"", "\\\"").Replace(msg)
	exec.Command("osascript", "-e",
		`display dialog "`+esc+`" buttons {"好"} default button 1 with title "MemoryCleaner" with icon `+icon).Run()
}
