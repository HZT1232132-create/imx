@echo off
chcp 65001 >nul
echo =============================================
echo   i.MX93 仓储标签异常鲁棒识别与分拣告警模拟系统
echo =============================================
echo.
echo 操作说明：
echo   按任意键 = 切换下一帧画面
echo   按 Q 键  = 退出模拟
echo.
echo 正在启动，请稍候...
echo.
wsl -d Ubuntu -- bash -c "export DISPLAY=:0 GDK_BACKEND=x11; cd \"/mnt/c/Users/Administrator/Desktop/nxp竞赛/imx93_sorting_sim/build\" && ./sorting_sim"
pause
