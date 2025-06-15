// Work Assistant Web Interface
class WorkAssistantApp {
    constructor() {
        this.websocket = null;
        this.connected = false;
        this.apiBase = window.location.origin;
        this.stats = {
            total_processed: 0,
            successful_extractions: 0,
            average_processing_time_ms: 0,
            average_confidence: 0
        };
        
        this.init();
    }
    
    init() {
        this.setupWebSocket();
        this.setupEventListeners();
        this.loadInitialData();
        this.startPeriodicUpdates();
    }
    
    setupWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        try {
            this.websocket = new WebSocket(wsUrl);
            
            this.websocket.onopen = () => {
                console.log('WebSocket connected');
                this.connected = true;
                this.updateConnectionStatus(true);
            };
            
            this.websocket.onclose = () => {
                console.log('WebSocket disconnected');
                this.connected = false;
                this.updateConnectionStatus(false);
                
                // Attempt to reconnect after 5 seconds
                setTimeout(() => this.setupWebSocket(), 5000);
            };
            
            this.websocket.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateConnectionStatus(false);
            };
            
            this.websocket.onmessage = (event) => {
                this.handleWebSocketMessage(event.data);
            };
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.updateConnectionStatus(false);
        }
    }
    
    handleWebSocketMessage(data) {
        try {
            const message = JSON.parse(data);
            
            switch (message.type) {
                case 'window_event':
                    this.updateWindowEvent(message.data);
                    break;
                case 'ocr_result':
                    this.updateOCRResult(message.data);
                    break;
                case 'ai_analysis':
                    this.updateAIAnalysis(message.data);
                    break;
                case 'stats_update':
                    this.updateStats(message.data);
                    break;
                default:
                    console.log('Unknown message type:', message.type);
            }
        } catch (error) {
            console.error('Failed to parse WebSocket message:', error);
        }
    }
    
    setupEventListeners() {
        // OCR mode selector
        const ocrModeSelect = document.getElementById('ocr-mode');
        if (ocrModeSelect) {
            ocrModeSelect.addEventListener('change', (e) => {
                this.changeOCRMode(e.target.value);
            });
        }
        
        // Refresh button
        const refreshBtn = document.getElementById('refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.loadInitialData();
            });
        }
        
        // Clear data button
        const clearBtn = document.getElementById('clear-data-btn');
        if (clearBtn) {
            clearBtn.addEventListener('click', () => {
                this.clearData();
            });
        }
        
        // Export data button
        const exportBtn = document.getElementById('export-data-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => {
                this.exportData();
            });
        }
    }
    
    async loadInitialData() {
        try {
            // Load application status
            const statusResponse = await fetch(`${this.apiBase}/api/status`);
            if (statusResponse.ok) {
                const status = await statusResponse.json();
                this.updateApplicationStatus(status);
            }
            
            // Load OCR statistics
            const statsResponse = await fetch(`${this.apiBase}/api/ocr/stats`);
            if (statsResponse.ok) {
                const stats = await statsResponse.json();
                this.updateStats(stats);
            }
            
            // Load recent activities
            const activitiesResponse = await fetch(`${this.apiBase}/api/activities/recent`);
            if (activitiesResponse.ok) {
                const activities = await activitiesResponse.json();
                this.updateRecentActivities(activities);
            }
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.showError('Failed to load application data');
        }
    }
    
    async changeOCRMode(mode) {
        try {
            const response = await fetch(`${this.apiBase}/api/ocr/mode`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ mode: mode })
            });
            
            if (response.ok) {
                this.showSuccess(`OCR mode changed to ${mode}`);
            } else {
                this.showError('Failed to change OCR mode');
            }
        } catch (error) {
            console.error('Failed to change OCR mode:', error);
            this.showError('Network error while changing OCR mode');
        }
    }
    
    async clearData() {
        if (!confirm('Are you sure you want to clear all data? This action cannot be undone.')) {
            return;
        }
        
        try {
            const response = await fetch(`${this.apiBase}/api/data/clear`, {
                method: 'POST'
            });
            
            if (response.ok) {
                this.showSuccess('Data cleared successfully');
                this.loadInitialData();
            } else {
                this.showError('Failed to clear data');
            }
        } catch (error) {
            console.error('Failed to clear data:', error);
            this.showError('Network error while clearing data');
        }
    }
    
    async exportData() {
        try {
            const response = await fetch(`${this.apiBase}/api/data/export`);
            
            if (response.ok) {
                const blob = await response.blob();
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `work_assistant_export_${new Date().toISOString().split('T')[0]}.json`;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                window.URL.revokeObjectURL(url);
                
                this.showSuccess('Data exported successfully');
            } else {
                this.showError('Failed to export data');
            }
        } catch (error) {
            console.error('Failed to export data:', error);
            this.showError('Network error while exporting data');
        }
    }
    
    updateConnectionStatus(connected) {
        const statusEl = document.getElementById('connection-status');
        if (statusEl) {
            statusEl.className = connected ? 'status-connected' : 'status-disconnected';
            statusEl.textContent = connected ? 'Connected' : 'Disconnected';
        }
    }
    
    updateApplicationStatus(status) {
        const statusEl = document.getElementById('app-status');
        if (statusEl) {
            statusEl.innerHTML = `
                <div><strong>OCR Engine:</strong> ${status.ocr_engine || 'Unknown'}</div>
                <div><strong>AI Engine:</strong> ${status.ai_engine || 'Unknown'}</div>
                <div><strong>Storage:</strong> ${status.storage_status || 'Unknown'}</div>
                <div><strong>Web Server:</strong> ${status.web_server || 'Running'}</div>
            `;
        }
    }
    
    updateStats(stats) {
        this.stats = { ...this.stats, ...stats };
        
        const elements = {
            'total-processed': this.stats.total_processed,
            'successful-extractions': this.stats.successful_extractions,
            'average-time': Math.round(this.stats.average_processing_time_ms) + 'ms',
            'average-confidence': Math.round(this.stats.average_confidence * 100) + '%'
        };
        
        Object.entries(elements).forEach(([id, value]) => {
            const el = document.getElementById(id);
            if (el) el.textContent = value;
        });
    }
    
    updateWindowEvent(event) {
        const container = document.getElementById('window-events');
        if (!container) return;
        
        const eventEl = document.createElement('div');
        eventEl.className = 'event-item';
        eventEl.innerHTML = `
            <div class="event-time">${new Date(event.timestamp).toLocaleTimeString()}</div>
            <div class="event-type">${event.type}</div>
            <div class="event-details">${event.window_title} (${event.process_name})</div>
        `;
        
        container.insertBefore(eventEl, container.firstChild);
        
        // Keep only last 10 events
        while (container.children.length > 10) {
            container.removeChild(container.lastChild);
        }
    }
    
    updateOCRResult(result) {
        const container = document.getElementById('ocr-results');
        if (!container) return;
        
        const resultEl = document.createElement('div');
        resultEl.className = 'ocr-result-item';
        resultEl.innerHTML = `
            <div class="result-time">${new Date(result.timestamp).toLocaleTimeString()}</div>
            <div class="result-confidence">Confidence: ${Math.round(result.confidence * 100)}%</div>
            <div class="result-text">${result.text.substring(0, 100)}${result.text.length > 100 ? '...' : ''}</div>
        `;
        
        container.insertBefore(resultEl, container.firstChild);
        
        // Keep only last 5 results
        while (container.children.length > 5) {
            container.removeChild(container.lastChild);
        }
    }
    
    updateAIAnalysis(analysis) {
        const container = document.getElementById('ai-analysis');
        if (!container) return;
        
        const analysisEl = document.createElement('div');
        analysisEl.className = 'analysis-item';
        analysisEl.innerHTML = `
            <div class="analysis-time">${new Date(analysis.timestamp).toLocaleTimeString()}</div>
            <div class="analysis-type">${analysis.content_type}</div>
            <div class="analysis-category">${analysis.work_category}</div>
            <div class="analysis-productive ${analysis.is_productive ? 'productive' : 'not-productive'}">
                ${analysis.is_productive ? '✅ Productive' : '⏱️ Break'}
            </div>
        `;
        
        container.insertBefore(analysisEl, container.firstChild);
        
        // Keep only last 5 analyses
        while (container.children.length > 5) {
            container.removeChild(container.lastChild);
        }
    }
    
    updateRecentActivities(activities) {
        const container = document.getElementById('recent-activities');
        if (!container) return;
        
        container.innerHTML = '';
        
        activities.forEach(activity => {
            const activityEl = document.createElement('div');
            activityEl.className = 'activity-item';
            activityEl.innerHTML = `
                <div class="activity-time">${new Date(activity.timestamp).toLocaleString()}</div>
                <div class="activity-window">${activity.window_title}</div>
                <div class="activity-content">${activity.content_summary}</div>
                <div class="activity-category">${activity.work_category}</div>
            `;
            
            container.appendChild(activityEl);
        });
    }
    
    startPeriodicUpdates() {
        // Update stats every 30 seconds
        setInterval(() => {
            if (this.connected) {
                this.loadInitialData();
            }
        }, 30000);
    }
    
    showSuccess(message) {
        this.showNotification(message, 'success');
    }
    
    showError(message) {
        this.showNotification(message, 'error');
    }
    
    showNotification(message, type) {
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        notification.textContent = message;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => {
                document.body.removeChild(notification);
            }, 300);
        }, 3000);
    }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.workAssistantApp = new WorkAssistantApp();
});