/**
 * @file BluetoothService.cpp
 * @author Chris Tacke
 * @brief Implementation of the BluetoothService class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "Logging.hpp"

#include "BluetoothService.hpp"
#include "BluetoothComponentName.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
const char *BluetoothService::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new BluetoothService object.
 */
BluetoothService::BluetoothService()
{
}

/**
 * @brief Destroy the BluetoothService object and release any allocated resources.
 */
BluetoothService::~BluetoothService()
{
    TRACE_MESSAGE("~BluetoothService: Enter");

    for (auto it = _characteristics.begin(); it != _characteristics.end(); ++it)
    {
        delete *it;
    }
    _characteristics.clear();

    TRACE_MESSAGE("~BluetoothService: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new BluetoothService object.
 *
 * @param name Name of the service.
 *
 * @param uuid UUID of the service.
 */
BluetoothService::BluetoothService(const char *name, const Uuid16 uuid)
{
    TRACE_MESSAGE("BluetoothService: Enter");

    Name.assign(name);
    Uuid = uuid;

    TRACE_MESSAGE("BluetoothService: Exit");
}

/**
 * @brief Add a characteristic to the service.
 *
 * @param characteristic Characteristic to be added.
 * @return BluetoothCharacteristic& Reference to the characteristic that has been added.
 */
BluetoothCharacteristic &BluetoothService::AddCharacteristic(BluetoothCharacteristic * const characteristic)
{
    TRACE_MESSAGE("AddCharacteristic: Enter");

    // do we already know about this characteristic?  If so, ignore
    bool exists = false;

    /*
    for (auto it = _characteristics.begin() ; it != _characteristics.end(); ++it)
    {
        auto chr = (BluetoothCharacteristic*)*it;

        // only check non-zero handles since the handle will be zero when we generate the graph
        if((characteristic->GetDeclaration().Handle != 0) && (chr->GetDeclaration().Handle == characteristic->GetDeclaration().Handle))
        {
            printf("Already exists.  Skipping");
            exists = true;
            break;
        }
    }
    */

    if (!exists)
    {
        /*
        // make sure handles are set
        if(characteristic->GetDeclaration().Handle == 0)
        {
            _handleManager->Assign(&characteristic->GetDeclaration());
        }
        if(characteristic->GetValue().Handle == 0)
        {
            _handleManager->Assign(&characteristic->GetValue());
        }

        // cross reference handles
        characteristic->GetDeclaration().ValueHandle = characteristic->GetValue().Handle;

        */

        // set the parent
        characteristic->ParentService = this;

        _characteristics.push_back(characteristic);

        if (OnCharacteristicAdded)
        {
            OnCharacteristicAdded(characteristic);
        }
    }

    TRACE_MESSAGE("%" PRIu32 " characteristics known", static_cast<uint32_t>(_characteristics.size()));

    TRACE_MESSAGE("AddCharacteristic: Exit");

    return *characteristic;
}
