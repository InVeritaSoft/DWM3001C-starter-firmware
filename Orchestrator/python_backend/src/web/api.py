"""
API Routes
REST API endpoints for test orchestrator
"""

from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel
from typing import Optional, Dict, Any, List
import asyncio


class TestConfig(BaseModel):
    """Test configuration model"""
    run_name: str
    jammer_label: str
    d_link: float
    env_type: str
    config: Dict[str, Any]


def create_api_router(node_a, node_b, test_runner, csv_logger, config) -> APIRouter:
    """Create API router with all endpoints"""
    router = APIRouter()
    
    @router.get("/status")
    async def get_status():
        """Get current test status"""
        return {
            "nodeA": {
                "state": node_a.get_state().value if node_a else "IDLE",
                "connected": node_a.is_connected() if node_a else False,
                "lastError": node_a.get_last_error() if node_a else None,
            },
            "nodeB": {
                "state": node_b.get_state().value if node_b else "IDLE",
                "connected": node_b.is_connected() if node_b else False,
                "lastError": node_b.get_last_error() if node_b else None,
            },
            "testRunning": test_runner.is_running if test_runner else False,
        }
    
    @router.get("/stats")
    async def get_stats():
        """Get latest stats from both nodes"""
        try:
            stats_a = node_a.get_stats_sync() if node_a else None
            stats_b = node_b.get_stats_sync() if node_b else None
            
            return {
                "nodeA": stats_a,
                "nodeB": stats_b,
            }
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.get("/history")
    async def get_history(
        page: int = Query(1, ge=1),
        limit: int = Query(100, ge=1, le=1000),
        file: Optional[str] = None
    ):
        """Get historical CSV data"""
        try:
            if file:
                # Read specific file
                data = await csv_logger.read_log_file(file)
                start = (page - 1) * limit
                end = start + limit
                return {
                    "data": data[start:end],
                    "total": len(data),
                    "page": page,
                    "limit": limit,
                }
            else:
                # List all log files
                files = csv_logger.list_log_files()
                return {"files": files}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.get("/test-plan")
    async def get_test_plan():
        """Get current test plan"""
        try:
            test_plan = config.get_test_plan()
            return {"tests": test_plan}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.post("/test/start")
    async def start_test(test_config: TestConfig):
        """Start test manually"""
        try:
            # Start test asynchronously (don't await - let it run in background)
            asyncio.create_task(run_test_background(test_config.dict()))
            
            # Return immediately - test is running in background
            return {"message": "Test started"}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    async def run_test_background(test: Dict[str, Any]):
        """Run test in background"""
        try:
            await test_runner.run_test(test)
        except Exception as error:
            print(f"Test execution error: {error}")
    
    @router.post("/test/stop")
    async def stop_test():
        """Stop current test"""
        try:
            await test_runner.stop_test()
            return {"message": "Test stopped"}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.post("/nodes/connect")
    async def connect_nodes():
        """Connect to both nodes"""
        try:
            results = await asyncio.gather(
                node_a.connect(),
                node_b.connect(),
                return_exceptions=True
            )
            
            return {
                "nodeA": "connected" if not isinstance(results[0], Exception) else f"error: {results[0]}",
                "nodeB": "connected" if not isinstance(results[1], Exception) else f"error: {results[1]}",
            }
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.post("/nodes/disconnect")
    async def disconnect_nodes():
        """Disconnect from both nodes"""
        try:
            await asyncio.gather(
                node_a.disconnect(),
                node_b.disconnect(),
                return_exceptions=True
            )
            return {"message": "Nodes disconnected"}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.post("/nodes/{node_id}/ping")
    async def ping_node(node_id: str):
        """Ping specific node"""
        try:
            node = node_a if node_id.upper() == "A" else node_b
            result = await node.ping()
            return {"success": result}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.post("/nodes/{node_id}/configure")
    async def configure_node(node_id: str, uwb_config: Dict[str, Any]):
        """Configure specific node"""
        try:
            node = node_a if node_id.upper() == "A" else node_b
            result = await node.configure(uwb_config)
            return {"success": result}
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    @router.get("/nodes/{node_id}/stats")
    async def get_node_stats(node_id: str):
        """Get stats from specific node"""
        try:
            node = node_a if node_id.upper() == "A" else node_b
            stats = await node.get_stats()
            return stats
        except Exception as error:
            raise HTTPException(status_code=500, detail=str(error))
    
    return router
