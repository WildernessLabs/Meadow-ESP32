using System.Threading.Tasks;

namespace MeadowApp
{
    public interface IMeadowTest
    {
        bool AsyncTest { get; }

        /// <summary>
        /// Name of the test class.
        /// </summary>
        /// <returns>Name of the test class/returns>
        string Name();

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        bool Setup();

        /// <summary>
        /// Execute the test synchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        bool Execute();

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        Task<bool> ExecuteAsync();

        /// <summary>
        /// Tear down the test restoring the system to a state where other tests can be run.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        bool Teardown();
    }
}