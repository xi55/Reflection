#include <Evently/Reflection.h>

class ConsumerType
{
public:
    int value_ = 0;
};

int main()
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();
    registry.registerClassName<ConsumerType>("ConsumerType");
    registry.registerField<ConsumerType>("ConsumerType", "value",
                                         &ConsumerType::value_);
    registry.registerClass<ConsumerType>("ConsumerType");
    registry.freeze();

    Evently::ObjectHandle object = registry.createInstance("ConsumerType");
    ConsumerType *value = object.get<ConsumerType>();
    if (value == nullptr)
    {
        return 1;
    }

    Evently::PropertySetterBase *setter =
        registry.getSetter("ConsumerType", "value");
    setter->set(value, Evently::Any(7));
    return value->value_ == 7 ? 0 : 2;
}
