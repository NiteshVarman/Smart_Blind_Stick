from fastapi import FastAPI
import pickle
import numpy as np
from pydantic import BaseModel

# Load the saved ARIMA model
with open("model.pkl", "rb") as f:
    model = pickle.load(f)

# Define API
app = FastAPI()

class ForecastRequest(BaseModel):
    steps: int

@app.post("/forecast")
def get_forecast(request: ForecastRequest):
    forecast = model.forecast(steps=request.steps)
    return {"forecast": forecast.tolist()}

