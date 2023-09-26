from setuptools import Extension, setup

setup(
    ext_modules=[
        Extension(
            name="cereggii.spam",
            sources=["src/cereggii/spammodule.c"],
        ),
    ]
)
